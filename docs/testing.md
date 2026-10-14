# Tests

## Tests automatisés

| Suite | Commande | Contenu | Dernier résultat |
|---|---|---|---|
| Service Windows | `scripts\build-windows.ps1` (CTest) | 102 tests : CBOR strict, SHA-256, ECDSA P-256, tuiles et autorisations à usage unique, stockage et révocation des associations, protocole et gestionnaire d'association, demandes de déverrouillage, état du PC, diagnostic, mise en forme de l'historique, annulation depuis la tuile | 102/102 le 14/10/2026 |
| Android | `android\gradlew.bat testDebugUnitTest` | 47 tests : CBOR strict, épinglage TLS, QR et échange d'association, enregistrement de l'association, demande d'autorisation et code de correspondance, signature DER vers `r‖s` | 47/47 le 14/10/2026 |

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
| Protection LSA activée dans la VM (`RunAsPPL=2`, comme sur le PC principal) puis performance : `scripts\recette-performance.ps1 -Since <heure> -IdleSeconds 600` | « LSASS.exe a démarré en tant que processus protégé avec le niveau : 4 » ; 7 déverrouillages sur 7 réussis ; preuve acceptée → résultat Windows : médiane 300 ms, maximum 1 005 ms (cible 3 s au 95e percentile) ; au repos pendant 10 minutes : processeur 0,005 % (cible < 1 %), mémoire privée 2 Mo (cible < 100 Mo). Écarts : 7 essais au lieu de 100 ; affichage de la demande sur le téléphone non chronométré | 14/10/2026 |
| Réseau coupé pendant une demande (T10) : câble de la VM débranché avant l'empreinte, rebranché ensuite | Rien ne s'ouvre ; l'ancienne preuve, arrivée après la reconnexion, est refusée (`EXPIRED`, 19:10:05) ; une nouvelle demande avec une nouvelle empreinte est nécessaire (session ouverte à 19:10:09) | 14/10/2026 |
| Mise en pause de la VM pendant 2 minutes, écran verrouillé (T06, en VM) | Après reprise, nouvelle demande depuis l'application et session ouverte (19:13:23 à 19:13:25). La vraie veille du PC reste à vérifier sur le PC principal | 14/10/2026 |
| Changement d'heure (T23) | **Non vérifié en VM** : les Additions invité de VirtualBox remettent l'heure aussitôt. Couvert par les tests automatiques (durée mesurée par l'horloge monotone du PC) ; les demandes ne sont gardées qu'en mémoire et ne survivent pas à un redémarrage | 14/10/2026 |
| Robustesse (T19, T20, T22, T25) : `scripts\recette-robustesse.ps1` en administrateur | Corps de 20 Ko `413`, CBOR invalide `400`, chemin inconnu `404`, adresse de 8 000 caractères `400`, PUT sans corps `411` (http.sys), preuve sans certificat `403`, TLS 1.2 refusé ; canal admin : trames de 4 Go, de type inconnu ou tronquées sans réponse ; canal de la tuile refusé à un administrateur ; 60 s de charge (11 350 requêtes, 50 en parallèle) : `403`, puis `503` d'http.sys et délais dépassés sous saturation, sans aucun redémarrage du service ; PIN et déverrouillage par l'application fonctionnels pendant la charge ; service de nouveau joignable aussitôt après | 14/10/2026 |
| Mot de passe du compte Microsoft changé en ligne (T11) | Windows accepte encore l'ancien mot de passe sur ce PC, y compris tapé à la main ; le déverrouillage par le téléphone continue donc de fonctionner (18:52, 18:53) ; `provision` avec le nouveau mot de passe : `OK`, puis session ouverte (18:55). Le refus attendu par le cahier ne se produit pas : comportement de Windows, pas du produit | 14/10/2026 |
| Changement de compte (T14) : depuis l'écran de `testeur`, « Demander » sur sa tuile puis « Déverrouiller » dans l'application ; demande depuis ma tuile puis passage sur `testeur` avant validation | « Aucun téléphone associé à ce compte » pour `testeur` ; l'application ouvre **ma** session (déverrouillage de la session 1, aucune ouverture de session pour `testeur`, D1) ; après correction A1 : la demande de la tuile est annulée dès qu'on la quitte (`unlock request from tile cancelled`, historique « Refusée (CANCELLED) »), l'application revient au bouton « Déverrouiller », le code disparaît de la tuile ; sans changement de compte, session ouverte | 14/10/2026 |
| Révocation avec le téléphone en mode avion (T13) | « Téléphone retiré » ; tuile « Aucun téléphone associé » ; à la reconnexion du téléphone, « Ce téléphone n'est plus associé à ce PC » | 14/10/2026 |
| « Hello uniquement » activé (T02) : `install.ps1`, `provision`, déverrouillage, redémarrage, déconnexion puis connexion par le téléphone | Message de l'installeur ; `provision` accepté ; `result status=0x00000000` au déverrouillage, après redémarrage et à la connexion après déconnexion (ouverture de session de type 5 à 18:12:34) ; après un redémarrage, Windows rouvre puis verrouille seul la session (option de connexion automatique après mise à jour) : le téléphone ne fait alors qu'un déverrouillage | 14/10/2026 |
| Paquet altéré (T26) : un octet ajouté au service dans une copie du paquet | « Empreinte SHA-256 incorrecte » avant toute modification ; `status` inchangé, Pixel 5 associé | 14/10/2026 |
| Identité et historique (F04, F12, F14) : `pair`, `revoke` et `history --clear` avec un mauvais puis le bon mot de passe ; `history` ; effacement puis nouveau déverrouillage par la tuile | Mauvais mot de passe : « Mot de passe refusé par Windows : rien n'a été modifié. » pour les trois ; bon : QR affiché, liste des téléphones, `OK` ; `history` en français (72 événements, 50 affichés) ; après effacement : « Historique effacé » seul, `status` remis à zéro, `cp.log` supprimé puis recréé ; puis demande depuis la tuile et « Session ouverte » | 14/10/2026 |
| Dossiers de données (T20) : mise à jour, compte standard `testeur`, dossier pré-créé par ce compte, puis lien vers `C:\Users\Public` | `PhoneUnlock` : SYSTEM, Administrateurs, service `(RX)` ; `tile-logs` : SYSTEM, Administrateurs ; `cp.log` écrit dans `tile-logs`, session ouverte ; `testeur` : création et lecture refusées ; installation refusée (« n'a pas été créé par un administrateur », puis « est un lien »), sans service installé ; après suppression du lien, installation neuve normale | 14/10/2026 |
| Diagnostic `status` : après mise à jour, déverrouillage réussi, demande expirée, service arrêté, terminal non élevé | Service, versions 0.1.0, adresse, « répond en TLS 1.3 », mot de passe et Pixel 5 ; « Pixel 5, réussi » ; « demande expirée (EXPIRED), Pixel 5 » ; « arrêté », HTTPS erreur 12029, « Service injoignable » ; sans élévation (`IsInRole` = `False`) : contrôles locaux affichés puis « Accès refusé » | 14/10/2026 |
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
1. Copier `build\package\PhoneUnlock-<version>` vers `C:\proto`, puis exécuter `install.ps1` en administrateur. Dossier partagé en lecture seule : `VBoxManage sharedfolder add win11-test --name package --hostpath E:\empreinte\build\package --transient --readonly`, puis `Copy-Item \\VBoxSvr\package\PhoneUnlock-<version> C:\proto\ -Recurse` ; vérifier `SHA256SUMS.txt`, car deux compilations peuvent porter le même numéro de version.
2. Lancer `PhoneUnlockService.exe pair --endpoint <IP de la VM>:47863` : le QR doit s'afficher.
3. Lancer `curl.exe -k -i -X POST --data-binary x -H "Content-Type: application/cbor" https://localhost:47863/v1/pair` : il faut une réponse `400` CBOR `INVALID_PROOF`. `403 UNSUPPORTED_TLS` signale que TLS 1.3 n'a pas été négocié. Sans corps, http.sys répond lui-même `411`.
4. `service.log` doit contenir `https server listening`.

### Procédures détaillées et tests restants


**Robustesse (VM)** : `powershell -ExecutionPolicy Bypass -File recette-robustesse.ps1` en administrateur ; pendant la charge, verrouiller puis ouvrir la session avec le PIN, puis déverrouiller avec l'application. Le script compare chaque réponse à l'attendu et vérifie que le processus du service n'a pas changé.

**Performance (VM)** : noter l'heure, faire les déverrouillages, puis `powershell -ExecutionPolicy Bypass -File recette-performance.ps1 -Since <heure> -IdleSeconds 600` en administrateur, sans toucher la VM pendant la mesure au repos.

**Installeur (VM)** : prendre un instantané, puis `install.ps1` sur l'installation existante (mise à jour), `uninstall.ps1` et redémarrage, puis contrôle de l'absence du service, des fichiers, des clés `{512D55BA-…}`, du certificat, de la règle de pare-feu (`Get-NetFirewallRule -DisplayName 'PhoneUnlock*'`), de la liaison TLS et de la réservation d'URL (`netsh http show sslcert` et `urlacl`). Échec provoqué : dans une autre fenêtre, `$l = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Any, 47863); $l.Start()`, puis `install.ps1` doit tout annuler.

**Mode tuile (VM et Pixel 5)** : verrouiller la VM, choisir la tuile Téléphone, cliquer « Demander au téléphone » ; la tuile affiche « Code NNNN » ; ouvrir PhoneUnlock, vérifier que le même code s'affiche, autoriser avec l'empreinte ; la session s'ouvre. Reste à vérifier : quatre demandes en une minute (« Trop de demandes » sur la tuile).

**Wi-Fi sans Internet, données mobiles du téléphone actives** (à faire) : Android peut alors faire passer les connexions par les données mobiles ; vérifier que l'application joint toujours le PC sur le Wi-Fi.

**PC réel sur le point d'accès dès l'écran de verrouillage** (à faire sur le PC principal, lot L6) : le réseau du point d'accès doit être connu pour tous les utilisateurs.

**Association avec le téléphone (VM et Pixel 5)**
1. Téléphone sur le même Wi-Fi que la VM. Installer l'application signée : `scripts\sign-apk.ps1` puis `adb install -r build\package\PhoneUnlock-<version>.apk`.
2. Dans la VM, en administrateur : `PhoneUnlockService.exe pair --endpoint <IP de la VM>:47863`.
3. Dans l'application : « Associer un PC », autoriser la caméra, scanner le QR.
4. Le téléphone et la console affichent le même code à 6 chiffres. Répondre `o` sur le PC, puis « Le code est identique : confirmer » et poser le doigt.
5. Preuves : la console affiche « Association enregistrée », `service.log` contient `history phone paired`, un fichier apparaît dans `C:\ProgramData\PhoneUnlock\state\pairings`, l'accueil de l'application affiche le PC et le compte.

Cas d'échec (validés le 14/10/2026) : confirmer sur le téléphone avant le PC (message « Réponds d'abord o sur le PC »), répondre `n` sur le PC, annuler l'empreinte, scanner un QR expiré (plus de 5 minutes), scanner un autre QR code.


### Test de soumission automatique (compilation de test uniquement)

`scripts\build-windows.ps1 -TestCommands`, installation dans la VM, puis `PhoneUnlockService.exe provision` et `PhoneUnlockService.exe authorize --delay 15`, puis Windows + L. La session doit se rouvrir seule.

## Recette du cahier des charges

Les tests T01 à T26 seront exécutés et tracés dans le rapport de recette (lot L6). Les procédures validées ci-dessus en couvrent déjà une partie, dans la VM et sur le Pixel 5 ; la correspondance test par test sera établie dans ce rapport.
