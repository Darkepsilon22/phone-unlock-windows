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

**À vérifier dans la VM** : la soumission automatique par la tuile (`CredentialsChanged` puis connexion automatique avec le crédential par défaut) depuis l'écran de verrouillage.

## D2 : état du PC en temps réel (13/10/2026)

L'application affiche l'état du PC tant qu'elle est ouverte : verrouillé, session ouverte, écran de connexion, injoignable. Le service suit les notifications de session Windows. Pas d'exécution en arrière-plan sur le téléphone (restriction du cahier conservée).

## D3 : fonctionnement sans connexion (13/10/2026)

**Besoin** : fonctionner dans un lieu sans aucune connexion.

| Mode | Lot | Principe |
|---|---|---|
| Wi-Fi commun sans Internet | v1 | Déjà prévu par le cahier |
| Point d'accès du téléphone | v1 | Le PC enregistre le point d'accès comme réseau connu, disponible avant ouverture de session ; les données mobiles ne sont pas nécessaires |
| Bluetooth Low Energy | ultérieur (F16) | Mêmes messages CBOR sur un autre transport ; sécurisation du canal à concevoir sans cryptographie maison |

**Écart réseau** : l'adresse du téléphone change selon le réseau (box ou point d'accès). La règle de pare-feu est donc limitée au sous-réseau local et au service, au lieu de la seule adresse du téléphone ; le TLS mutuel épinglé reste obligatoire.

**À vérifier dans la VM** : ouverture de session du compte Microsoft sans Internet (identifiants en cache ; le mot de passe doit avoir déjà servi une fois sur ce PC).
