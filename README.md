# Déverrouillage Windows par téléphone

Projet personnel gratuit (cahier des charges v1.1, `docs/Cahier_des_charges.pdf`).
Le téléphone Android autorise, par biométrie forte, une demande de connexion signée ;
un Credential Provider Windows soumet ensuite les identifiants au système.

## Structure

| Dossier | Contenu | Lot |
|---|---|---|
| `protocol/` | Spécification du protocole (CBOR, ECDSA P-256, TLS mutuel) | L2 |
| `android/` | Application Kotlin / Compose / AndroidX Biometric | L3 |
| `windows/credential-provider/` | DLL COM C++ chargée par LogonUI | L1, L4 |
| `windows/service/` | Service Windows C++ (réseau, preuve, secrets) | L4 |
| `windows/config-tool/` | Assistant C# .NET (diagnostic, révocation, historique) | L4 |
| `windows/installer/` | Installeur WiX | L5 |
| `tests/` | Recette T01 à T26 et preuves | L6 |
| `scripts/` | Outils de build et de test | |

## Règles de sécurité du dépôt

- Aucune clé de signature, aucun mot de passe, aucun secret dans le dépôt.
- Le Credential Provider n'est installé **que dans la VM `win11-test`** (VirtualBox), jamais sur le PC principal avant validation du jalon L1.
- Avant chaque essai dans la VM : restaurer ou prendre un instantané (`01-propre-hello`).

## Environnement de développement

- VS Code, Build Tools 2022 (MSVC 14.44), Windows SDK 10.0.26100, CMake, vcpkg (`E:\dev\vcpkg`)
- .NET SDK 8 / 10, WiX (`dotnet tool`), WinDbg
- Android Studio, JDK 21, Gradle, adb
- VM : `E:\VMs\win11-test` (Windows 11 Famille, compte Microsoft, Hello uniquement activé)

## État

- [x] L0 : outils, VM de test, instantané propre
- [x] L1 : prototype Credential Provider dans la VM, validé si Hello uniquement est désactivé ([rapport](docs/rapport-jalon-L1.md))
- [x] L2 : spécification du protocole ([SPEC.md](protocol/SPEC.md))
- [ ] L3 : application Android (étape 1 faite : clé StrongBox et signature biométrique validées sur Pixel 5)
