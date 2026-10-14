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

- La tuile et le service ne sont installés **que dans la VM** `win11-test` (VirtualBox, sans Hyper-V). `install.ps1` refuse toute autre machine sans l'option `-AllowPhysicalMachine`, et refuse toujours une compilation de test hors VM.
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
| Plantage du service (`Stop-Process -Force`) puis désinstallation complète et redémarrage | Écran de verrouillage normal, tuile « Service PhoneUnlock injoignable », PIN fonctionnel ; après désinstallation : service, tuile, DLL, liaison TLS, pare-feu et certificat absents, PIN fonctionnel | 14/10/2026 |
| APK release signé : `revoke` sur le PC, désinstallation de la version debug, installation de `PhoneUnlock-0.1.0.apk`, nouvelle association, déverrouillage | `apksigner verify` : schéma v3, signataire `CN=PhoneUnlock` ; `run-as` refusé (« package not debuggable ») ; une seule entrée dans `phones` ; session ouverte depuis l'application et depuis la tuile (`status=0x00000000`) | 14/10/2026 |
| Installeur : mise à jour de l'installation existante (`install.ps1`) | « Certificat du PC conservé », « Liaison TLS conservée. », Pixel 5 toujours dans `phones` ; session ouverte depuis l'application et depuis la tuile sans nouvelle association (`status=0x00000000`) | 14/10/2026 |
| Désinstallation (`uninstall.ps1`) puis redémarrage | Service, fichiers, clés de registre, certificat, pare-feu, liaison TLS et réservation d'URL absents ; seul `logs` reste ; PIN fonctionnel | 14/10/2026 |
| Installation en échec : port 47863 occupé par un autre programme | « Le service ne répond pas sur le port 47863 », annulation ; mêmes contrôles qu'après désinstallation ; `https server setup failed error=32` | 14/10/2026 |
| Installation neuve, `provision`, association, déverrouillage | Nouveau certificat, « Association enregistrée », session ouverte depuis l'application et depuis la tuile (`status=0x00000000`) | 14/10/2026 |
| Révocation : `phones`, `revoke` ; téléphone refusé aussitôt (« n'est plus associé »), tuile « Aucun téléphone associé » ; nouvelle association du même téléphone, une seule entrée dans `phones`, déverrouillage de nouveau possible | Messages attendus, session ouverte après réassociation | 14/10/2026 |
| Mode tuile : « Demander au téléphone », même code à 4 chiffres sur la tuile et le téléphone, autorisation par empreinte ; refus sur le téléphone (rien ne s'ouvre) ; expiration après 60 s (la tuile revient à l'état initial) | Session ouverte ; aucune ouverture en cas de refus ou d'expiration | 14/10/2026 |
| Déverrouillage sans aucune connexion : point d'accès du Pixel 5, données mobiles coupées, PC hôte et VM sur ce point d'accès | L'application retrouve la VM sur le réseau du point d'accès (adresse 10.166.107.177 ajoutée à `pairing.cbor`), état « Verrouillé » puis session ouverte | 14/10/2026 |
| Déverrouillage depuis l'application : écran de verrouillage et écran de connexion après redémarrage ; empreinte annulée, 4ᵉ demande en une minute, plus de 30 s avant l'empreinte | Session ouverte (`history unlock result … status=0x00000000`), puis messages « Empreinte non confirmée », « Trop de demandes », « Demande expirée » sans ouverture | 14/10/2026 |
| Vérification du mot de passe par `provision` : mauvais refusé, bon accepté, en ligne et VM débranchée | « Mot de passe refusé par Windows », puis `OK` | 14/10/2026 |
| Association de bout en bout avec le Pixel 5 (code identique des deux côtés, confirmation sur le PC puis par empreinte) | Console « Association enregistrée », `pairing.cbor` créé sur le téléphone | 14/10/2026 |
| Cas d'échec de l'association : téléphone avant le PC, refus sur le PC, empreinte annulée, QR expiré, QR étranger | Messages attendus sur le téléphone et la console, aucune association enregistrée | 14/10/2026 |
| Serveur HTTPS, TLS 1.3, QR code d'association | `https server listening`, `400` CBOR `INVALID_PROOF`, `https POST /v1/pair certificate=0` | 13/10/2026 |

**Serveur HTTPS et QR code (VM)**
1. Copier `build\package\PhoneUnlock-<version>` vers `C:\proto`, puis exécuter `install.ps1` en administrateur.
2. Lancer `PhoneUnlockService.exe pair --endpoint <IP de la VM>:47863` : le QR doit s'afficher.
3. Lancer `curl.exe -k -i -X POST --data-binary x -H "Content-Type: application/cbor" https://localhost:47863/v1/pair` : il faut une réponse `400` CBOR `INVALID_PROOF`. `403 UNSUPPORTED_TLS` signale que TLS 1.3 n'a pas été négocié. Sans corps, http.sys répond lui-même `411`.
4. `service.log` doit contenir `https server listening`.

### Procédures à exécuter


**Installeur (VM)** : prendre un instantané, puis `install.ps1` sur l'installation existante (mise à jour), `uninstall.ps1` et redémarrage, puis contrôle de l'absence du service, des fichiers, des clés `{512D55BA-…}`, du certificat, de la règle de pare-feu (`Get-NetFirewallRule -DisplayName 'PhoneUnlock*'`), de la liaison TLS et de la réservation d'URL (`netsh http show sslcert` et `urlacl`). Échec provoqué : dans une autre fenêtre, `$l = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Any, 47863); $l.Start()`, puis `install.ps1` doit tout annuler.

**Mode tuile (VM et Pixel 5)** : verrouiller la VM, choisir la tuile Téléphone, cliquer « Demander au téléphone » ; la tuile affiche « Code NNNN » ; ouvrir PhoneUnlock, vérifier que le même code s'affiche, autoriser avec l'empreinte ; la session s'ouvre. Reste à vérifier : quatre demandes en une minute (« Trop de demandes » sur la tuile).

**Wi-Fi sans Internet, données mobiles du téléphone actives** (à faire) : Android peut alors faire passer les connexions par les données mobiles ; vérifier que l'application joint toujours le PC sur le Wi-Fi.

**PC réel sur le point d'accès dès l'écran de verrouillage** (à faire sur le PC principal, lot L6) : le réseau du point d'accès doit être connu pour tous les utilisateurs.

**Association avec le téléphone (VM et Pixel 5)**
1. Téléphone sur le même Wi-Fi que la VM. Installer l'application signée : `scripts\sign-apk.ps1` puis `adb install -r build\package\PhoneUnlock-<version>.apk`.
2. Dans la VM, en administrateur : `PhoneUnlockService.exe pair --endpoint <IP de la VM>:47863`.
3. Dans l'application : « Associer un PC », autoriser la caméra, scanner le QR.
4. Le téléphone et la console affichent le même code à 6 chiffres. Répondre `o` sur le PC, puis « Le code est identique : confirmer » et poser le doigt.
5. Preuves : la console affiche « Association enregistrée », `service.log` contient `pairing completed`, un fichier apparaît dans `C:\ProgramData\PhoneUnlock\state\pairings`, l'accueil de l'application affiche le PC et le compte.

Cas d'échec (validés le 14/10/2026) : confirmer sur le téléphone avant le PC (message « Réponds d'abord o sur le PC »), répondre `n` sur le PC, annuler l'empreinte, scanner un QR expiré (plus de 5 minutes), scanner un autre QR code.


### Test de soumission automatique (compilation de test uniquement)

`scripts\build-windows.ps1 -TestCommands`, installation dans la VM, puis `PhoneUnlockService.exe provision` et `PhoneUnlockService.exe authorize --delay 15`, puis Windows + L. La session doit se rouvrir seule.

## Recette du cahier des charges

Les tests T01 à T26 seront tracés dans le rapport de recette (lot L6). Couverture actuelle, partielle : T05 et T06 (connexion après redémarrage, déverrouillage) avec mot de passe saisi ou fourni par le service, T07 et T08 sur le téléphone seul (la signature n'est pas encore reliée au PC). Aucun autre test de recette n'a été exécuté.
