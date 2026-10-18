/*
 * Copyright (C) 2019 Medusalix
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "dongle.h"
#include "../utils/log.h"
#include "xbox/reconnect.h"

Dongle::Dongle(
    std::unique_ptr<UsbDevice> usbDevice
) : Mt76(std::move(usbDevice)), stopThreads(false)
{
    Log::info("Dongle initialized");
    reconnectAddress = xbox_last_controller();
    if (reconnectAddress.size()) {
        automaticReconnect = true;
        if (!setPairingStatus(true)) throw Mt76Exception("Failed to advertise Xbox reconnect");
        Log::info("Advertising reconnect for the saved Xbox controller");
    }
#ifndef XBOX_RADIO_TEST
    threads.emplace_back(
        &Dongle::readBulkPackets,
        this,
        MT_EP_READ
    );
    threads.emplace_back(
        &Dongle::readBulkPackets,
        this,
        MT_EP_READ_PACKET
    );
#endif
}

Dongle::~Dongle()
{
    stopThreads = true;

    // Wait for all threads to shut down
    for (std::thread &thread : threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
}

void Dongle::handleControllerConnect(Bytes address, bool reassociation)
{
    std::lock_guard<std::mutex> lock(controllerMutex);

    // Reconnect advertising accepts only the previously announced controller.
    // The physical adapter button still enables pairing a different gamepad.
    if (automaticReconnect && address != reconnectAddress) return;

    Log::debug("Controller %s request: %s", reassociation ? "reassociation" : "association",
              Log::formatBytes(address).c_str());
    uint8_t wcid = associateClient(address, reassociation);

    if (wcid == 0)
    {
        Log::error("Failed to associate controller");

        return;
    }

    GipDevice::SendPacket sendPacket = std::bind(
        &Dongle::sendClientPacket,
        this,
        wcid,
        address,
        std::placeholders::_1
    );

    if (!controllers[wcid - 1]) {
        uint16_t vendor = 0, product = 0;
        xbox_controller_identity(address, vendor, product);
        controllers[wcid - 1].reset(new Controller(sendPacket, [this, address](uint16_t vid, uint16_t pid) {
            xbox_remember_controller(address, vid, pid);
            // Association precedes GIP startup. Changing the beacon here,
            // rather than immediately after association, keeps the radio
            // available until the controller has completed that handshake.
            if (automaticReconnect && address == reconnectAddress) {
                automaticReconnect = false;
                setPairingStatus(false);
                Log::info("Saved Xbox controller reconnected without the adapter button");
            }
        }, vendor, product));
        Log::info("Controller '%d' connected", wcid);
    }
}

void Dongle::handleControllerDisconnect(uint8_t wcid)
{
    // Ignore invalid WCIDs
    if (wcid == 0 || wcid > MT_WCID_COUNT)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(controllerMutex);

    // Ignore unconnected controllers
    if (!controllers[wcid - 1])
    {
        return;
    }

    controllers[wcid - 1].reset();

    if (!removeClient(wcid))
    {
        Log::error("Failed to remove controller");

        return;
    }

    Log::info("Controller '%d' disconnected", wcid);
    reconnectAddress = xbox_last_controller();
    if (reconnectAddress.size()) {
        automaticReconnect = true;
        setPairingStatus(true);
    }
}

void Dongle::handleControllerPair(Bytes address, const Bytes &packet)
{
    std::lock_guard<std::mutex> lock(controllerMutex);
    if (automaticReconnect && address != reconnectAddress) return;
    // Ignore invalid packets
    if (packet.size() < sizeof(ReservedFrame))
    {
        return;
    }

    const ReservedFrame *frame = packet.toStruct<ReservedFrame>();

    // Type 0x01 is for pairing requests
    if (frame->type != 0x01)
    {
        return;
    }

    if (!pairClient(address))
    {
        Log::error("Failed to pair controller");

        return;
    }

    if (!setPairingStatus(false))
    {
        Log::error("Failed to disable pairing");

        return;
    }

    Log::debug(
        "Controller paired: %s",
        Log::formatBytes(address).c_str()
    );
}

void Dongle::handleControllerPacket(uint8_t wcid, const Bytes &packet)
{
    // Invalid WCID
    if (wcid == 0 || wcid > MT_WCID_COUNT)
    {
        return;
    }

    // Ignore invalid or empty packets
    if (packet.size() <= sizeof(QosFrame) + sizeof(uint16_t))
    {
        return;
    }

    // Skip 2 bytes of padding
    const Bytes data(packet, sizeof(QosFrame) + sizeof(uint16_t));

    std::lock_guard<std::mutex> lock(controllerMutex);

    // Ignore unconnected controllers
    if (!controllers[wcid - 1])
    {
        return;
    }

    if (!controllers[wcid - 1]->handlePacket(data))
    {
        Log::error("Error handling packet for controller '%d'", wcid);
    }
}

void Dongle::handleWlanPacket(const Bytes &packet)
{
    // Ignore invalid or empty packets
    if (packet.size() <= sizeof(RxWi) + sizeof(WlanFrame))
    {
        return;
    }

    const RxWi *rxWi = packet.toStruct<RxWi>();
    const WlanFrame *wlanFrame = packet.toStruct<WlanFrame>(sizeof(RxWi));

    const Bytes source(
        wlanFrame->source,
        wlanFrame->source + macAddress.size()
    );
    const Bytes destination(
        wlanFrame->destination,
        wlanFrame->destination + macAddress.size()
    );

    static_assert(sizeof(RxWi) == 32 && sizeof(WlanFrame) == 24,
                  "MT76 radio frames require packed GCC bitfields");
#ifdef DEBUG
    // Bounded diagnostics for pairing: include frames rejected by destination.
    if (wlanFrame->frameControl.type == MT_WLAN_MANAGEMENT) {
        static std::atomic<unsigned> diagnostics(0);
        if (diagnostics++ < 32)
            Log::info("WLAN management subtype=%u len=%zu source=%s destination=%s match=%d",
                      wlanFrame->frameControl.subtype, packet.size(),
                      Log::formatBytes(source).c_str(), Log::formatBytes(destination).c_str(),
                      destination == macAddress);
    }
#endif

    // Packet has wrong destination address
    if (destination != macAddress)
    {
        return;
    }
#ifdef DEBUG
    if (wlanFrame->frameControl.type == MT_WLAN_MANAGEMENT) {
        static std::atomic<unsigned> directedDiagnostics(0);
        if (directedDiagnostics++ < 64) {
            const Bytes prefix(packet.raw(), packet.raw() + std::min<size_t>(packet.size(), 96));
            Log::info("Directed WLAN subtype=%u frame=%s", wlanFrame->frameControl.subtype,
                      Log::formatBytes(prefix).c_str());
        }
    }
#endif

    uint8_t type = wlanFrame->frameControl.type;
    uint8_t subtype = wlanFrame->frameControl.subtype;

    if (type == MT_WLAN_MANAGEMENT)
    {
        switch (subtype)
        {
            case MT_WLAN_ASSOCIATION_REQ:
                handleControllerConnect(source);
                break;
            case MT_WLAN_REASSOCIATION_REQ:
                handleControllerConnect(source, true);
                break;

            // Only kept for compatibility with 1537 controllers
            // They associate, disassociate and associate again during pairing
            // Disassociations happen without triggering EVT_CLIENT_LOST
            case MT_WLAN_DISASSOCIATION:
                handleControllerDisconnect(rxWi->wcid);
                break;

            // Reserved frames are used for different purposes
            // Most of them are yet to be discovered
            case MT_WLAN_RESERVED:
                const Bytes innerPacket(
                    packet,
                    sizeof(RxWi) + sizeof(WlanFrame)
                );

                handleControllerPair(source, innerPacket);
                break;
        }
    }

    else if (type == MT_WLAN_DATA && subtype == MT_WLAN_QOS_DATA)
    {
        const Bytes innerPacket(
            packet,
            sizeof(RxWi) + sizeof(WlanFrame)
        );

        handleControllerPacket(rxWi->wcid, innerPacket);
    }
}

void Dongle::handleBulkData(const Bytes &data)
{
    // Ignore invalid or empty data
    if (data.size() <= sizeof(RxInfoGeneric) + sizeof(uint32_t))
    {
        return;
    }

    // Skip packet end marker (4 bytes, identical to header)
    const RxInfoGeneric *rxInfo = data.toStruct<RxInfoGeneric>();
    // Sequence one is a channel-scan command response, not a controller frame.
    // Its extra metadata otherwise shifts every parsed WLAN address by 4 bytes.
    if (rxInfo->port == CPU_RX_PORT && data.toStruct<RxInfoCommand>()->sequence == 1)
        return;
    const Bytes packet(data, sizeof(RxInfoGeneric), sizeof(uint32_t));

    if (rxInfo->port == CPU_RX_PORT)
    {
        const RxInfoCommand *info = data.toStruct<RxInfoCommand>();

        switch (info->eventType)
        {
            case EVT_CHANNEL_SELECTION:
                // Channel selection is asynchronous and can replace the
                // initial beacon. Publish reconnect advertising afterwards.
                {
                    std::lock_guard<std::mutex> lock(controllerMutex);
                    if (automaticReconnect) {
                        setPairingStatus(true);
                        Log::info("Refreshed saved-controller advertising after channel selection");
                    }
                }
                break;

            case EVT_BUTTON_PRESS:
                // An explicit button press permits pairing a different pad.
                {
                    std::lock_guard<std::mutex> lock(controllerMutex);
                    automaticReconnect = false;
                    setPairingStatus(true);
                }
                break;

            case EVT_PACKET_RX:
                handleWlanPacket(packet);
                break;

            case EVT_CLIENT_LOST:
                // Packet is guaranteed not to be empty
                handleControllerDisconnect(packet[0]);
                break;
        }
    }

    else if (rxInfo->port == WLAN_PORT)
    {
        const RxInfoPacket *info = data.toStruct<RxInfoPacket>();

        if (info->is80211)
        {
            handleWlanPacket(packet);
        }
    }
}

void Dongle::readBulkPackets(uint8_t endpoint)
{
    FixedBytes<USB_MAX_BULK_TRANSFER_SIZE> buffer;
#ifdef DEBUG
    unsigned diagnostics = 0;
#endif

    while (!stopThreads)
    {
        int transferred = usbDevice->bulkRead(endpoint, buffer);

        // Bulk read failed
        if (transferred < 0)
        {
            break;
        }

        if (transferred > 0)
        {
            Bytes data = buffer.toBytes(transferred);
#ifdef DEBUG
            if (diagnostics++ < 24) {
                const Bytes prefix(data.raw(), data.raw() + std::min<size_t>(data.size(), 40));
                Log::info("USB IN 0x%02x bytes=%d header=%s", endpoint | 0x80,
                          transferred, Log::formatBytes(prefix).c_str());
            }
#endif

            handleBulkData(data);
        }
    }
}
