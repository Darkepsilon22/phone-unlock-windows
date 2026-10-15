# Empreinte : déverrouillage Windows par téléphone

Projet personnel et gratuit : ouvrir ou déverrouiller une session Windows 11 (compte Microsoft) avec l'empreinte d'un téléphone Android, sur le réseau local et sans service cloud. Le téléphone signe une demande après biométrie, et une tuile Windows soumet alors le mot de passe du compte, conservé chiffré par un service local. Windows reste seul juge de l'ouverture de session.

**État : prototype.** Ne pas installer sur un PC principal avant la recette (lot L6). Lire [docs/security.md](docs/security.md) puis [docs/install.md](docs/install.md) avant tout usage.

## Documentation

| Document | Contenu |
|---|---|
| [docs/Cahier_des_charges.pdf](docs/Cahier_des_charges.pdf) | Besoin et exigences (v1.1) |
| [protocol/SPEC.md](protocol/SPEC.md) | Protocole téléphone ↔ PC |
| [docs/install.md](docs/install.md) | Installation, mise à jour, diagnostic, désinstallation, récupération |
| [docs/architecture.md](docs/architecture.md) | Modules, flux, frontières de sécurité |
| [docs/security.md](docs/security.md) | Modèle de menace, secrets, limites, récupération |
| [docs/testing.md](docs/testing.md) | Tests automatisés et procédures manuelles |
| [docs/recette.md](docs/recette.md) | Rapport de recette : exigences, tests T01 à T26, anomalies, écarts |
| [docs/decisions.md](docs/decisions.md) | Écarts au cahier et choix techniques (D1 à D12) |
| [docs/rapport-jalon-L1.md](docs/rapport-jalon-L1.md) | Preuve de faisabilité Windows |
| [docs/third-party.md](docs/third-party.md) | Composants tiers et licences |

## Structure

| Dossier | Contenu |
|---|---|
| `android/` | Application Kotlin, Jetpack Compose, AndroidX Biometric |
| `windows/shared/` | Code C++ commun : canal local, journal, mémoire effacée |
| `windows/credential-provider/` | Tuile Windows (DLL COM chargée par LogonUI) |
| `windows/service/` | Service Windows : coffre, association, API HTTPS, commandes d'administration, tests |
| `protocol/` | Spécification du protocole |
| `scripts/` | Compilation Windows, signature de l'APK, installeur, désinstalleur, essais de robustesse et de performance |
| `docs/` | Documentation |

## Prérequis (gratuits)

- Windows 11 x64, VS Code
- Visual Studio Build Tools 2022 (MSVC, Windows SDK, CMake)
- vcpkg : chemin donné par la variable `VCPKG_ROOT` (par défaut `E:\dev\vcpkg` dans le script)
- Android Studio ou SDK Android, JDK 21 ; Gradle est fourni par le wrapper
- VirtualBox pour les tests Windows (pas d'Hyper-V)
- Un téléphone Android 11 ou plus avec empreinte de classe forte

## Compiler et tester

```powershell
scripts\build-windows.ps1                  # compile la tuile et le service, lance les tests, prépare build\package\PhoneUnlock-<version>
scripts\build-windows.ps1 -TestCommands    # variante de test (version suffixée -test) avec la commande authorize, refusée hors VM

cd android
.\gradlew.bat assembleDebug testDebugUnitTest
cd ..
scripts\sign-apk.ps1                       # compile la version release et la signe avec ta clé locale
adb install -r build\package\PhoneUnlock-<version>.apk   # téléphone branché, débogage USB activé
```

Ces commandes sont aussi disponibles dans VS Code (Terminal → Exécuter la tâche).

## Avancement

- [x] L0 : outils, VM de test
- [x] L1 : faisabilité Windows, compte Microsoft en ligne et hors ligne ; la condition « Hello uniquement désactivé » a été levée le 14/10/2026 (D12)
- [x] L2 : spécification du protocole
- [x] L3 : application Android : clés StrongBox, association, état du PC en temps réel, déverrouillage par bouton et par tuile, validés sur Pixel 5
- [x] L4 : service et tuile : coffre, canaux locaux, soumission automatique, serveur HTTPS en TLS 1.3, association, déverrouillage depuis le téléphone et depuis la tuile, validés en VM
- [x] L4 bis : révocation d'un téléphone depuis le PC, validée en VM
- [x] L5 : installeur avec annulation sur erreur, APK signé, diagnostic `status`, protection des dossiers de données, documentation
- [ ] L6 : recette en VM terminée ([docs/recette.md](docs/recette.md)) ; installation et vérifications sur le PC principal à faire

## Limitations connues

« Hello uniquement » peut rester activé (D12, à confirmer sur le PC principal). Le mot de passe du compte est conservé chiffré sur le PC. L'application doit être ouverte pendant le déverrouillage. Détails dans [docs/security.md](docs/security.md) ; secours dans [docs/install.md](docs/install.md), section 8.
