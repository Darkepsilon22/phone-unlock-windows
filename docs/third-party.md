# Composants tiers et licences

Relevé du 14/10/2026. Les licences Android sont lues dans les fichiers POM du cache Gradle (ou dans leur POM parent quand l'artefact n'en déclare pas), pour les 140 modules de `releaseRuntimeClasspath`. Les versions sont figées dans `windows/vcpkg.json` et `android/gradle/libs.versions.toml`.

## Embarqués dans les binaires

### Windows

| Composant | Version | Licence | Usage |
|---|---|---|---|
| Nayuki QR Code generator (vcpkg `nayuki-qr-code-generator`) | 1.8.0 | MIT | QR code d'association affiché dans la console, lié statiquement |
| Bibliothèque C++ de MSVC (CRT statique) | Build Tools 2022 | Licence Microsoft Visual Studio | Exécution du service et de la tuile |

Tout le reste vient des API de Windows (http.sys, CNG, DPAPI, WinHTTP, Credential Provider) : aucune autre bibliothèque n'est embarquée.

### Android

| Composant | Version | Licence |
|---|---|---|
| AndroidX : core-ktx, activity-compose, fragment-ktx, biometric | 1.19.1, 1.13.0, 1.9.1, 1.1.0 | Apache 2.0 |
| Jetpack Compose (BOM), ui, material3 | 2026.09.00 | Apache 2.0 |
| CameraX : camera2, lifecycle, view | 1.6.2 | Apache 2.0 ; `camera-core` déclare aussi BSD-3-Clause |
| ZXing core | 3.5.4 | Apache 2.0 |
| Bibliothèque standard Kotlin | 2.4.21 | Apache 2.0 |

Dépendances transitives : toutes sous Apache 2.0 (AndroidX, kotlinx, Guava, Dagger, Error Prone, J2ObjC, JSpecify, javax et jakarta.inject, annotations JetBrains, AutoValue, JSR 305), sauf `org.checkerframework:checker-qual`, sous licence MIT.

## Outils de compilation et de test (non embarqués)

| Outil | Licence |
|---|---|
| Gradle (wrapper) 9.8.1, Android Gradle Plugin 9.4.1 | Apache 2.0 |
| JUnit 4.13.2 (tests unitaires Android) | Eclipse Public License 1.0 |
| vcpkg | MIT |
| CMake | BSD-3-Clause |
| Visual Studio Build Tools 2022, Windows SDK, SDK Android | Licences Microsoft et Google, gratuites pour cet usage |

## Obligations

Pour un usage personnel, rien n'est à publier. Si l'APK ou les binaires étaient un jour distribués à d'autres personnes, il faudrait joindre les textes des licences Apache 2.0, MIT et BSD-3-Clause ainsi que les avis de copyright des composants embarqués.

Pour refaire le relevé Android : `gradlew :app:dependencies --configuration releaseRuntimeClasspath`, puis lire la balise `<licenses>` du POM de chaque module dans `%USERPROFILE%\.gradle\caches\modules-2\files-2.1`.
