# Translations

Cervin ships its Qt interface in French: `fr.ts` is compiled into the executable
(`ENABLE_QT_TRANSLATION`, on by default) and chosen from the system locale, or from the language
setting.

`fr.ts` started from the French translation Fritchii removed (`14557706b`, from Eden's Transifex
project), with Eden/yuzu renamed Cervin. To pick up new or changed strings from the sources,
run Qt's `lupdate` from the repository root, then translate the entries marked
`type="unfinished"`:

```
lupdate src/yuzu src/qt_common -ts dist/languages/fr.ts -no-obsolete -locations relative
```
