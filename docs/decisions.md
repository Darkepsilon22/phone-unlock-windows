# Décisions du projet

Écarts et précisions par rapport au cahier des charges v1.1, validés par l'utilisatrice.

## D1 : déverrouillage lancé depuis le téléphone (13/10/2026)

**Décision** : les deux modes coexistent.
- Mode téléphone : dans l'application, bouton « Déverrouiller », empreinte, le PC s'ouvre sans toucher au clavier.
- Mode PC (cahier) : tuile « Téléphone », bouton « Demander », empreinte sur le téléphone.

**Écart** : F06 et S12 imposaient une initiation explicite sur le PC.

**Risque accepté** : une personne présente devant le PC accède à la session si l'utilisatrice déverrouille à distance par erreur. Le même réseau ne prouve pas la proximité.

**Mesures** :
- la demande n'est créée que si le PC est verrouillé et que la tuile du compte associé est affichée ;
- elle est liée à ce verrouillage précis (`context_id`) et expire en 30 secondes ;
- le nom du PC et du compte sont affichés et signés avant l'empreinte ;
- chaque déverrouillage par téléphone est inscrit à l'historique du PC ;
- quotas inchangés (3 demandes par minute).

**Vérifié dans la VM le 13/10/2026** : la soumission automatique par la tuile (`CredentialsChanged` puis connexion automatique avec le crédential par défaut) fonctionne depuis l'écran de verrouillage, session ouverte en moins d'une seconde. Le mode téléphone lui-même (bouton dans l'application) reste à réaliser.

## D2 : état du PC en temps réel (13/10/2026)

L'application affiche l'état du PC tant qu'elle est ouverte : verrouillé, session ouverte, écran de connexion, injoignable. Le service suit les notifications de session Windows. Pas d'exécution en arrière-plan sur le téléphone (restriction du cahier conservée).

## D3 : fonctionnement sans connexion (13/10/2026)

**Besoin** : fonctionner dans un lieu sans aucune connexion.

| Mode | Lot | Principe |
|---|---|---|
| Wi-Fi commun sans Internet | v1 | Déjà prévu par le cahier |
| Point d'accès du téléphone | v1 | Le PC enregistre le point d'accès comme réseau connu, disponible avant ouverture de session ; les données mobiles ne sont pas nécessaires |
| Bluetooth Low Energy | ultérieur (F16) | Mêmes messages CBOR sur un autre transport ; sécurisation du canal à concevoir sans cryptographie maison |

**Écart réseau** : l'adresse du téléphone change selon le réseau (box ou point d'accès). La règle de pare-feu est donc limitée au port 47863/TCP et au sous-réseau local, au lieu de la seule adresse du téléphone ; elle ne peut pas viser le processus du service car `http.sys` s'exécute dans le noyau. Le TLS mutuel épinglé reste obligatoire.

**Vérifié dans la VM le 13/10/2026** : ouverture et déverrouillage du compte Microsoft sans réseau avec `KERB_INTERACTIVE_UNLOCK_LOGON` (voir D6), à condition que Windows ait déjà mis le mot de passe en cache lors d'une connexion en ligne par mot de passe.

## D4 : serveur HTTPS de Windows (13/10/2026)

**Décision** : le service expose l'API avec l'API HTTP Server de Windows (`http.sys`), liée au certificat d'identité du PC par `netsh http add sslcert` avec négociation du certificat client.

**Raisons** : TLS géré par Windows et mis à jour avec lui, aucune bibliothèque de chiffrement à embarquer, gratuit. **Contreparties** : liaison et réservation d'URL à créer à l'installation ; règle de pare-feu limitée au port (voir D3).

## D5 : TLS 1.3 exigé dans le code (13/10/2026)

**Décision** : le service refuse toute requête dont le protocole négocié n'est pas TLS 1.3, avec le code `UNSUPPORTED_TLS` ajouté à la spécification. L'option `disabletls12` de `netsh` est tentée à l'installation mais n'est qu'une défense supplémentaire, car elle n'existe pas sur toutes les versions de Windows.

## D6 : un seul format de sérialisation (13/10/2026)

**Décision** : la tuile utilise uniquement `KERB_INTERACTIVE_UNLOCK_LOGON` (domaine `MicrosoftAccount` pour un compte Microsoft). Les formats `CredPackAuthenticationBuffer` et le réglage de registre qui permettait de les choisir ont été supprimés.

**Raison** : le format avec fournisseur d'identité exigeait Internet (`0xC000023C`) ; le format retenu fonctionne en ligne et hors ligne (rapport L1). Les anciens formats restent dans l'historique Git.

## D7 : commande de test réservée aux compilations de test (13/10/2026)

**Décision** : la commande `authorize`, qui simule l'autorisation du téléphone, n'est compilée qu'avec l'option CMake `PHONEUNLOCK_TEST_COMMANDS` (`build-windows.ps1 -TestCommands`). La compilation normale refuse la requête correspondante.

**Raison** : dans une compilation normale, elle permettrait à un administrateur d'ouvrir la session sans téléphone.

## D8 : mini-exécuteur de tests C++ (13/10/2026)

**Décision** : les tests du service utilisent un exécuteur de 50 lignes (`tests/test_runner.*`) intégré à CTest, plutôt qu'un framework.

**Raison** : aucune dépendance supplémentaire, suffisant pour des tests unitaires. À revoir si les besoins dépassent `TEST` et `CHECK`.
