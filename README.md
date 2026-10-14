# Empreinte : déverrouillage Windows par téléphone

Projet personnel et gratuit : ouvrir ou déverrouiller une session Windows 11 (compte Microsoft) avec l'empreinte d'un téléphone Android, sur le réseau local et sans service cloud. Le téléphone signe une demande après biométrie, et une tuile Windows soumet alors le mot de passe du compte, conservé chiffré par un service local. Windows reste seul juge de l'ouverture de session.

**État : prototype.** Ne pas installer sur un PC principal. Lire [docs/security.md](docs/security.md) avant tout usage.

## Documentation

| Document | Contenu |
|---|---|
| [docs/Cahier_des_charges.pdf](docs/Cahier_des_charges.pdf) | Besoin et exigences (v1.1) |
| [protocol/SPEC.md](protocol/SPEC.md) | Protocole téléphone ↔ PC |
| [docs/architecture.md](docs/architecture.md) | Modules, flux, frontières de sécurité |
| [docs/security.md](docs/security.md) | Modèle de menace, secrets, limites, récupération |
| [docs/testing.md](docs/testing.md) | Tests automatisés et procédures manuelles |
| [docs/decisions.md](docs/decisions.md) | Écarts au cahier et choix techniques (D1 à D10) |
| [docs/rapport-jalon-L1.md](docs/rapport-jalon-L1.md) | Preuve de faisabilité Windows |

## Structure

| Dossier | Contenu |
|---|---|
| `android/` | Application Kotlin, Jetpack Compose, AndroidX Biometric |
| `windows/shared/` | Code C++ commun : canal local, journal, mémoire effacée |
| `windows/credential-provider/` | Tuile Windows (DLL COM chargée par LogonUI) |
| `windows/service/` | Service Windows : coffre, association, API HTTPS, commandes d'administration, tests |
| `protocol/` | Spécification du protocole |
| `scripts/` | Compilation Windows et installation dans la VM de test |
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
scripts\build-windows.ps1                  # compile la tuile et le service, lance les 36 tests, prépare build\deploy
scripts\build-windows.ps1 -TestCommands    # variante de test avec la commande authorize (jamais sur un vrai PC)

cd android
.\gradlew.bat assembleDebug testDebugUnitTest
.\gradlew.bat installDebug                 # téléphone branché, débogage USB activé
```

Ces commandes sont aussi disponibles dans VS Code (Terminal → Exécuter la tâche).

## Avancement

- [x] L0 : outils, VM de test
- [x] L1 : faisabilité Windows, compte Microsoft en ligne et hors ligne, à condition de désactiver Hello uniquement
- [x] L2 : spécification du protocole
- [ ] L3 : application Android (clé StrongBox, association, état du PC et déverrouillage par bouton validés ; mode tuile validé en VM)
- [ ] L4 : service et tuile (coffre, canaux locaux et soumission automatique validés ; serveur HTTPS en TLS 1.3, association et déverrouillage par le téléphone validés en VM ; mode tuile validé en VM)
- [x] L4 bis : révocation d'un téléphone depuis le PC, validée en VM
- [ ] L5 : installeur signé
- [ ] L6 : recette

## Limitations connues

Hello uniquement doit être désactivé. Le mot de passe du compte est conservé chiffré sur le PC. L'application doit être ouverte pendant le déverrouillage. Détails dans [docs/security.md](docs/security.md).
