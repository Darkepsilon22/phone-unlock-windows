# Sécurité

## Principe

La biométrie autorise une clé du téléphone, la clé autorise une demande précise, et **Windows reste seul juge** de l'ouverture de session. Le produit ne contourne aucun mécanisme Windows. Il soumet le mot de passe du compte, conservé chiffré sur le PC.

## Risque principal accepté

Le mot de passe du compte Microsoft est **récupérable par le service** : il est chiffré par DPAPI dans le contexte du compte de service, et seul ce compte peut le déchiffrer. Un attaquant qui obtient les droits administrateur ou SYSTEM peut le récupérer, et donc accéder au compte Microsoft en ligne, pas seulement au PC. Ce risque est inhérent à l'architecture choisie dans le cahier des charges et doit être accepté explicitement avant toute utilisation sur le PC principal. **Accepté par l'utilisatrice le 15/10/2026 (D13).**

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

### Dossiers de données

Tout utilisateur peut créer des dossiers dans `C:\ProgramData`. Avant la correction du 14/10/2026, un compte standard présent avant l'installation pouvait créer `ProgramData\PhoneUnlock` à l'avance pour en rester propriétaire, puis remplacer `state` par un dossier à lui contenant une fausse association ; il pouvait aussi faire de `ProgramData\PhoneUnlockProto`, où la tuile écrivait son journal en tant que SYSTEM, un lien vers un autre emplacement. Désormais :

| Dossier | Droits |
|---|---|
| `ProgramData\PhoneUnlock` | SYSTEM et Administrateurs ; service en lecture ; héritage coupé |
| `secrets`, `state`, `logs` | SYSTEM et Administrateurs ; service en modification |
| `tile-logs` (journal de la tuile) | SYSTEM et Administrateurs seulement, pour que le service ne puisse pas y placer de lien |

L'installeur refuse de continuer, sans rien modifier, si l'un de ces dossiers est un lien ou n'appartient ni à SYSTEM ni aux Administrateurs, et refuse une installation neuve si `secrets` ou `state` existent déjà. Les programmes ne créent plus aucun dossier eux-mêmes. L'ancien dossier `PhoneUnlockProto` est supprimé sans suivre les liens (`rmdir`). Vérifié en VM avec un compte standard : création refusée dans ces dossiers, installation refusée sur un dossier pré-créé par ce compte puis sur un lien.

La clé de signature de l'APK (`%USERPROFILE%\.phoneunlock\release.jks`, PKCS12, RSA 4096) reste hors du dépôt ; son mot de passe est tapé à chaque signature. Une autre clé empêche toute mise à jour de l'application installée : il faut alors la désinstaller, ce qui efface ses clés, révoquer le téléphone sur le PC puis l'associer à nouveau.

## Menaces du cahier des charges

| Menace | État | Mesures |
|---|---|---|
| S01 rejeu | Fait | `request_id` et nonce aléatoires signés, 30 s monotones, consommation unique, une demande active par compte ; rejeu refusé (tests et VM) |
| S02 usurpation | Fait | Empreintes épinglées, TLS 1.3 mutuel, code à 6 chiffres comparé sur les deux écrans puis confirmé sur le PC et par empreinte ; QR étranger et confirmation dans le mauvais ordre refusés (VM) |
| S03 téléphone perdu | Fait | Biométrie par opération (validée sur Pixel 5) ; révocation immédiate depuis le PC (`revoke`, réservée aux administrateurs), validée en VM |
| S04 commande forgée | Fait | Signature ECDSA vérifiée par le PC sur la demande qu'il reconstruit lui-même ; champ modifié ou signature falsifiée refusés (tests) |
| S05 mauvais compte | Fait | SID et `context_id` de la tuile signés et revérifiés ; un nouvel écran de verrouillage invalide la demande (tests) |
| S06 vol du secret | Fait | DPAPI, ACL, compte de service dédié, journaux sans secret ; dossier `ProgramData\PhoneUnlock` protégé lui aussi, propriétaire et liens contrôlés à l'installation (voir « Dossiers de données ») |
| S07 détournement de l'IPC | Fait | Deux canaux à ACL distinctes, vérification du serveur par la tuile, échec fermé si le descripteur de sécurité ne peut pas être construit |
| S08 saturation | Fait | 16 Ko, 8 requêtes simultanées, 3 essais d'association, 3 demandes de déverrouillage par minute et par compte (VM) |
| S09 mise à jour malveillante | Fait | Empreintes SHA-256 vérifiées par l'installeur avant et après copie, fichiers modifiables seulement par les administrateurs (droits contrôlés), compilation de test refusée hors VM, annulation complète en cas d'échec ; APK release signé (schéma v3) avec une clé locale hors dépôt, non débogable (D11, VM et Pixel 5) |
| S10 nouvelle biométrie | Fait | Clé invalidée à l'enrôlement (Android) |
| S11 blocage du PC | Fait | Fournisseurs Windows conservés, tuile sans appel bloquant ; service arrêté brutalement : écran de verrouillage normal, « Service PhoneUnlock injoignable », PIN fonctionnel ; désinstallation complète sans trace, PIN fonctionnel après redémarrage (VM, 14/10/2026) |
| S12 mauvaise demande | Fait | Tuile du compte affichée exigée, noms du PC et du compte signés et montrés avant l'empreinte ; depuis la tuile, code à 4 chiffres identique sur la tuile et le téléphone (VM) ; historique dans `service.log` (lignes `history`) |

## Conditions et limites connues

- **Hello uniquement** peut rester activé : connexion et déverrouillage vérifiés en VM le 14/10/2026, contrairement au rapport L1 (D12). À confirmer sans Internet et sur le PC principal ; si Windows refuse avec `0xC000015B`, le désactiver.
- Le mot de passe doit avoir servi **une fois en ligne** sur le PC pour que l'ouverture hors ligne fonctionne.
- `provision` fait vérifier le mot de passe par Windows (`LogonUserW`, identité `MicrosoftAccount\adresse` lue par `NetUserGetInfo` niveau 24) avant de l'enregistrer, en ligne comme hors ligne. Chaque mot de passe erroné compte comme une tentative ratée pour Windows. `pair`, `revoke` et `history --clear` font la même vérification (F04, F12) : une session ouverte laissée sans surveillance ne suffit pas pour associer un téléphone, en retirer un ou effacer l'historique.
- Après un changement du mot de passe du compte Microsoft, Windows accepte encore l'ancien sur ce PC (constaté en VM le 14/10/2026, T11) ; le service, qui envoie l'ancien, continue donc de fonctionner. Après un changement fait pour une raison de sécurité, relancer `provision` pour que seul le nouveau soit conservé.
- **Pixel 5** : plus de correctifs de sécurité depuis novembre 2023.
- Le même Wi-Fi ne prouve pas la proximité. Un déverrouillage depuis le téléphone ouvre la session pour la personne devant le PC (D1).
- BitLocker n'a pas été testé avec le produit : il a été désactivé dans la VM à cause des instantanés VirtualBox.
- La commande de test `authorize` existe uniquement dans les compilations `-TestCommands` (D7). Ne jamais déployer une telle compilation sur le PC principal.

## Récupération

- Le PIN, Windows Hello et le mot de passe restent toujours disponibles : la tuile ne remplace aucun fournisseur.
- Désactiver le produit : `uninstall.ps1` en administrateur. La tuile, le service, la liaison TLS, le pare-feu, le certificat, les associations et les secrets sont retirés.
- En VM : restaurer un instantané (`VBoxManage snapshot win11-test restore <nom>`).
- Procédures complètes (tuile seule, environnement de récupération, clé APK perdue) : [install.md](install.md), section 8.
- Le produit n'est jamais installé sur le PC principal avant validation complète, et uniquement avec ton accord.
