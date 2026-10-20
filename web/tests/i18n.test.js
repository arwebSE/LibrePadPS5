import { afterEach, describe, expect, it, vi } from 'vitest';
import { initialLanguage, languages, matchLanguage, translate } from '../src/composables/useI18n';
import { messages, translations } from '../src/locales/messages';

afterEach(() => { localStorage.clear(); vi.restoreAllMocks(); });

describe('translation catalogs', () => {
  it('provides every message and matching placeholders in every supported language', () => {
    const placeholders = text => [...text.matchAll(/\{\w+\}/g)].map(match => match[0]).sort();
    expect(new Set(translations.map(row => row[0])).size).toBe(translations.length);
    expect(Object.keys(messages)).toEqual(languages.map(language => language.code));
    for (const row of translations) {
      expect(row, row[0]).toHaveLength(languages.length);
      for (const text of row) {
        expect(text, row[0]).toBeTypeOf('string');
        expect(text.trim(), row[0]).not.toBe('');
        expect(placeholders(text), row[0]).toEqual(placeholders(row[0]));
      }
    }
  });

  it('interpolates entire messages without changing user values or unknown server messages', () => {
    expect(translate('ja', 'Assigned to {name}', { name: 'Player $& {slot}' })).toBe('Player $& {slot} に割り当て済み');
    expect(translate('sv', 'New server message')).toBe('New server message');
    expect(translate('unknown', 'Language')).toBe('Language');
    expect(translate('de', 'constructor')).toBe('constructor');
  });
});

describe('language selection', () => {
  it.each([
    ['ja-JP', 'ja'], ['es-MX', 'es'], ['de-AT', 'de'], ['fr-CA', 'fr'], ['sv-SE', 'sv'],
    ['zh-CN', 'zh-Hans'], ['zh-SG', 'zh-Hans'], ['zh-Hans-CN', 'zh-Hans'], ['zh-Hans-TW', 'zh-Hans'], ['zh', 'zh-Hans'],
    ['zh-TW', null], ['zh-HK', null], ['zh-Hant', null], ['ko-KR', null],
  ])('matches %s to %s', (tag, expected) => expect(matchLanguage(tag)).toBe(expected));

  it('uses a saved choice before browser preferences and ignores invalid saved choices', () => {
    vi.spyOn(navigator, 'languages', 'get').mockReturnValue(['ko-KR', 'fr-CA', 'de-DE']);
    expect(initialLanguage()).toBe('fr');
    localStorage.setItem('librepad-language', 'ja');
    expect(initialLanguage()).toBe('ja');
    localStorage.setItem('librepad-language', 'broken');
    expect(initialLanguage()).toBe('fr');
  });

  it('falls back to English for unsupported languages and handles blocked storage', () => {
    vi.spyOn(navigator, 'languages', 'get').mockReturnValue(['ko-KR', 'zh-Hant']);
    expect(initialLanguage()).toBe('en');
    vi.spyOn(Storage.prototype, 'getItem').mockImplementation(() => { throw new Error('Blocked'); });
    vi.spyOn(navigator, 'languages', 'get').mockReturnValue(['sv-SE']);
    expect(initialLanguage()).toBe('sv');
  });
});
