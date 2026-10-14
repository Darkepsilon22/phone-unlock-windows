# Tests

## Tests automatisés

| Suite | Commande | Contenu | Dernier résultat |
|---|---|---|---|
| Service Windows | `scripts\build-windows.ps1` (CTest) | 36 tests : CBOR strict, SHA-256, ECDSA P-256, courtier d'autorisations, stockage des associations, protocole et gestionnaire d'association | 36/36 le 13/10/2026 |
| Android | `android\gradlew.bat testDebugUnitTest` | 4 tests : conversion de signature DER vers `r‖s` | 4/4 le 13/10/2026 |

Les tâches VS Code « Windows : compiler et tester » et « Android : tests unitaires » lancent ces suites. Les workflows GitHub Actions `windows-build.yml` et `android-build.yml` se déclenchent **manuellement** (onglet Actions), pour préserver le quota gratuit.

Non couverts automatiquement : `http.sys`, les canaux locaux, LogonUI, DPAPI et la biométrie. Ils dépendent de Windows ou du matériel et sont vérifiés à la main ci-dessous.

## Vérifications manuelles

### Règles

- La tuile et le service ne sont installés **que dans la VM** `win11-test` (VirtualBox, sans Hyper-V). Les scripts `vm-install-*` refusent toute autre machine.
- Prendre un instantané avant chaque installation.
- La carte réseau de la VM est en **accès par pont** sur le Wi-Fi de l'hôte : la VM a sa propre adresse, comme un vrai PC. En NAT, VirtualBox conserve l'adresse d'origine des connexions redirigées, que la règle de pare-feu « réseau local » de la VM rejette (constaté le 14/10/2026 dans `pfirewall.log`).
- Une capture d'écran ne suffit pas : la preuve est le journal (`cp.log`, `service.log`) avec les codes renvoyés par Windows.
- Le TPM virtuel de VirtualBox peut être réinitialisé après un instantané : Windows annonce alors « Votre PIN n'est plus disponible ». Constaté le 13/10/2026 au démarrage, avant toute installation (journal Hello : `État : TPM Cleared`). Se connecter par mot de passe et reconfigurer le PIN.

### Procédures validées

| Procédure | Preuve | Date |
|---|---|---|
| Connexion, déverrouillage, après redémarrage (en ligne) | `result status=0x00000000`, rapport L1 | 12/10/2026 |
| Mêmes cas sans réseau | Rapport L1, complément | 13/10/2026 |
| Soumission automatique depuis le service | `automatic=1` puis `status=0x00000000` | 13/10/2026 |
| Clé StrongBox, signature biométrique, refus sans empreinte, annulation | Application de diagnostic sur Pixel 5 | 13/10/2026 |
| Association de bout en bout avec le Pixel 5 (code identique des deux côtés, confirmation sur le PC puis par empreinte) | Console « Association enregistrée », `pairing.cbor` créé sur le téléphone | 14/10/2026 |
| Serveur HTTPS, TLS 1.3, QR code d'association | `https server listening`, `400` CBOR `INVALID_PROOF`, `https POST /v1/pair certificate=0` | 13/10/2026 |

**Serveur HTTPS et QR code (VM)**
1. Copier `build\deploy` vers `C:\proto`, puis exécuter `vm-install-cp.ps1` et `vm-install-service.ps1` en administrateur.
2. Lancer `PhoneUnlockService.exe pair --endpoint <IP de la VM>:47863` : le QR doit s'afficher.
3. Lancer `curl.exe -k -i -X POST --data-binary x -H "Content-Type: application/cbor" https://localhost:47863/v1/pair` : il faut une réponse `400` CBOR `INVALID_PROOF`. `403 UNSUPPORTED_TLS` signale que TLS 1.3 n'a pas été négocié. Sans corps, http.sys répond lui-même `411`.
4. `service.log` doit contenir `https server listening`.

### Procédures à exécuter

**Association avec le téléphone (VM et Pixel 5)**
1. Téléphone sur le même Wi-Fi que la VM. Installer l'application : `android\gradlew.bat installDebug`.
2. Dans la VM, en administrateur : `PhoneUnlockService.exe pair --endpoint <IP de la VM>:47863`.
3. Dans l'application : « Associer un PC », autoriser la caméra, scanner le QR.
4. Le téléphone et la console affichent le même code à 6 chiffres. Répondre `o` sur le PC, puis « Le code est identique : confirmer » et poser le doigt.
5. Preuves : la console affiche « Association enregistrée », `service.log` contient `pairing completed`, un fichier apparaît dans `C:\ProgramData\PhoneUnlock\state\pairings`, l'accueil de l'application affiche le PC et le compte.

Cas d'échec à vérifier : confirmer sur le téléphone avant le PC (message « Réponds d'abord o sur le PC »), répondre `n` sur le PC, annuler l'empreinte, scanner un QR expiré (plus de 5 minutes), scanner un autre QR code.

**Désinstallation** : `vm-uninstall-cp.ps1` et `vm-uninstall-service.ps1`, redémarrage, puis connexion par PIN.

### Test de soumission automatique (compilation de test uniquement)

`scripts\build-windows.ps1 -TestCommands`, installation dans la VM, puis `PhoneUnlockService.exe provision` et `PhoneUnlockService.exe authorize --delay 15`, puis Windows + L. La session doit se rouvrir seule.

## Recette du cahier des charges

Les tests T01 à T26 seront tracés dans le rapport de recette (lot L6). Couverture actuelle, partielle : T05 et T06 (connexion après redémarrage, déverrouillage) avec mot de passe saisi ou fourni par le service, T07 et T08 sur le téléphone seul (la signature n'est pas encore reliée au PC). Aucun autre test de recette n'a été exécuté.
