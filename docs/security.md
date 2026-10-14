# Sécurité

## Principe

La biométrie autorise une clé du téléphone, la clé autorise une demande précise, et **Windows reste seul juge** de l'ouverture de session. Le produit ne contourne aucun mécanisme Windows. Il soumet le mot de passe du compte, conservé chiffré sur le PC.

## Risque principal accepté

Le mot de passe du compte Microsoft est **récupérable par le service** : il est chiffré par DPAPI dans le contexte du compte de service, et seul ce compte peut le déchiffrer. Un attaquant qui obtient les droits administrateur ou SYSTEM peut le récupérer, et donc accéder au compte Microsoft en ligne, pas seulement au PC. Ce risque est inhérent à l'architecture choisie dans le cahier des charges et doit être accepté explicitement avant toute utilisation sur le PC principal.

## Clés et secrets

| Élément | Emplacement | Protection |
|---|---|---|
| Mot de passe Windows | `ProgramData\PhoneUnlock\secrets\<SID>.bin` | DPAPI du service, entropie = SID, ACL : SYSTEM, Administrateurs, service |
| Identité du PC | Magasin `LocalMachine\My`, `CN=PhoneUnlock PC` | ECDSA P-256, clé non exportable |
| Associations | `ProgramData\PhoneUnlock\state\pairings` | Données publiques (empreintes, clés publiques, SID) ; ACL restreintes |
| Clé d'autorisation du téléphone | Android Keystore | StrongBox si disponible, empreinte forte à chaque signature, invalidée à l'ajout d'une empreinte, exclue des sauvegardes |
| Clé de transport du téléphone | Android Keystore | ECDSA P-256 non exportable, sans biométrie, sert uniquement de certificat client TLS |
| Association côté téléphone | Fichier privé de l'application `pairing.cbor` | Données publiques (identifiant et empreinte du PC, adresses, noms) ; exclu des sauvegardes |

Le téléphone n'accepte que le PC dont l'empreinte figure dans le QR code : son gestionnaire de confiance TLS compare `SHA-256(SubjectPublicKeyInfo)` du certificat présenté à l'empreinte épinglée, et la vérification du nom d'hôte est volontairement remplacée par cette comparaison (avertissements Lint `CustomX509TrustManager` et `BadHostnameVerifier` acquittés en connaissance de cause). Le client n'active que TLS 1.3.
| Mot de passe en mémoire | Service et tuile | Copies limitées, effacées (`SecureZeroMemory`), protégé par `CredProtect` avant sérialisation |

Aucun secret n'est écrit dans les journaux ni passé en argument de processus.

## Menaces du cahier des charges

| Menace | État | Mesures |
|---|---|---|
| S01 rejeu | Fait (mode téléphone) | `request_id` et nonce aléatoires signés, 30 s monotones, consommation unique, une demande active par compte ; rejeu refusé (tests et VM) |
| S02 usurpation | Partiel | Empreintes épinglées, TLS 1.3 mutuel, code à 6 chiffres (côté PC fait, non validé en VM) |
| S03 téléphone perdu | Fait | Biométrie par opération (validée sur Pixel 5) ; révocation immédiate depuis le PC (`revoke`, réservée aux administrateurs), validée en VM |
| S04 commande forgée | Fait (mode téléphone) | Signature ECDSA vérifiée par le PC sur la demande qu'il reconstruit lui-même ; champ modifié ou signature falsifiée refusés (tests) |
| S05 mauvais compte | Fait (mode téléphone) | SID et `context_id` de la tuile signés et revérifiés ; un nouvel écran de verrouillage invalide la demande (tests) |
| S06 vol du secret | Fait | DPAPI, ACL, compte de service dédié, journaux sans secret |
| S07 détournement de l'IPC | Fait | Deux canaux à ACL distinctes, vérification du serveur par la tuile, échec fermé si le descripteur de sécurité ne peut pas être construit |
| S08 saturation | Fait | 16 Ko, 8 requêtes simultanées, 3 essais d'association, 3 demandes de déverrouillage par minute et par compte (VM) |
| S09 mise à jour malveillante | À faire | Signature des paquets (lot L5) |
| S10 nouvelle biométrie | Fait | Clé invalidée à l'enrôlement (Android) |
| S11 blocage du PC | Partiel | Fournisseurs Windows conservés et tuile sans appel bloquant (constaté en VM) ; désinstallation et plantage du service pas encore testés |
| S12 mauvaise demande | Partiel | Mode téléphone : tuile du compte affichée exigée, noms du PC et du compte signés et montrés avant l'empreinte, historique dans `service.log` (lignes `history`). Mode tuile (code à 4 chiffres) : lot 2b-2 |

## Conditions et limites connues

- **Hello uniquement** doit être désactivé sur le PC, sinon Windows refuse la connexion par mot de passe (`0xC000015B`, rapport L1).
- Le mot de passe doit avoir servi **une fois en ligne** sur le PC pour que l'ouverture hors ligne fonctionne.
- `provision` fait vérifier le mot de passe par Windows (`LogonUserW`, identité `MicrosoftAccountdresse` lue par `NetUserGetInfo` niveau 24) avant de l'enregistrer, en ligne comme hors ligne. Chaque mot de passe erroné compte comme une tentative ratée pour Windows.
- **Pixel 5** : plus de correctifs de sécurité depuis novembre 2023.
- Le même Wi-Fi ne prouve pas la proximité. Un déverrouillage depuis le téléphone ouvre la session pour la personne devant le PC (D1).
- BitLocker n'a pas été testé avec le produit : il a été désactivé dans la VM à cause des instantanés VirtualBox.
- La commande de test `authorize` existe uniquement dans les compilations `-TestCommands` (D7). Ne jamais déployer une telle compilation sur le PC principal.

## Récupération

- Le PIN, Windows Hello et le mot de passe restent toujours disponibles : la tuile ne remplace aucun fournisseur.
- Désactiver le produit : `vm-uninstall-cp.ps1` puis `vm-uninstall-service.ps1`. Le service, la liaison TLS, le pare-feu, le certificat, les associations et les secrets sont retirés.
- En VM : restaurer un instantané (`VBoxManage snapshot win11-test restore <nom>`).
- Le produit n'est jamais installé sur le PC principal avant validation complète, et uniquement avec ton accord.
