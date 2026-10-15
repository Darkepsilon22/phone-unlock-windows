# Rapport de recette (lot L6)

Recette menée du 14 au 15 octobre 2026 dans la VM de test, avant toute installation sur le PC principal. Les preuves détaillées (journaux, codes Windows, heures) sont dans [testing.md](testing.md), procédure par procédure. Une capture d'écran seule n'a jamais servi de preuve.

## Configuration

| Élément | Version |
|---|---|
| PC de test | VM VirtualBox `win11-test`, Windows 11 Famille 26200.9711, compte Microsoft jetable, PIN, « Hello uniquement » activé, protection LSA activée (`RunAsPPL=2`), réseau par pont sur le Wi-Fi |
| Téléphone | Google Pixel 5, Android 14 (correctif 2023-11-05), empreinte de classe forte, StrongBox |
| Service et tuile | 0.1.0 (empreintes du paquet dans `SHA256SUMS.txt`) |
| Application | 0.1.0, APK release signé, schéma v3 |
| Tests automatiques | 102 tests C++ et 47 tests Kotlin, tous réussis |

## Exigences fonctionnelles

| Exigence | Tests | Statut |
|---|---|---|
| F01 Diagnostic de compatibilité | T02, `install.ps1` (Windows, x64, Smart App Control, Hello uniquement), `status`, refus clair de l'application sans empreinte forte | Fait |
| F02 Installation avec retour arrière | T01, T16, échec provoqué (port occupé) | Fait |
| F03 QR temporaire et code sur les deux écrans | T03, T04 | Fait |
| F04 Liaison au SID après vérification de l'identité | T03 ; `pair` exige le mot de passe vérifié par Windows | Fait |
| F05 Tuile « Téléphone » | T01, T05 | Fait |
| F06 Demande explicite et limitée depuis la tuile | Mode tuile, 60 s (D10) ; demande lancée depuis le téléphone en plus (D1) | Fait, avec l'écart D1 |
| F07 PC, compte et demande affichés sur le mobile | Nom du PC, compte et code de correspondance affichés avant l'empreinte | Fait |
| F08 Biométrie forte et signature liée à la demande | T07, T08, T12 | Fait |
| F09 Preuve vérifiée, une seule soumission | T07, T17, T18, T21 | Fait |
| F10 Autorisation et ouverture réelle distinguées | Historique « Autorisée » puis « Session ouverte » ou « Refusée par Windows » ; `WINDOWS_REJECTED` | Fait |
| F11 Annulation, expiration, reprise sans blocage | T08, T09, T10, T14, T15 | Fait |
| F12 Révocation après authentification récente | T13 ; `revoke` exige le mot de passe | Fait |
| F13 Perte de clé, changement de mot de passe, réassociation | T11, T12, guide (`install.md`, sections 4 et 8) | Fait, avec la limite Windows de T11 |
| F14 Historique et effacement | `history`, `history --clear` (mot de passe exigé) | Fait |
| F15 Installer, mettre à jour, désinstaller sans toucher aux méthodes de secours | T16, plantage et désinstallation (S11), récupération | Fait |

## Tests du cahier des charges

| Test | Résultat |
|---|---|
| T01 Installation | Réussi en VM (installation neuve et mise à jour) et sur le PC principal le 15/10/2026 ; connexion habituelle par PIN conservée |
| T02 Compte non compatible | « Hello uniquement » finalement **compatible** (D12). Compte sans mot de passe et Smart App Control bloquant : non testables dans la VM (le compte Microsoft a un mot de passe, Smart App Control ne peut pas être réactivé) ; l'installeur contrôle Smart App Control |
| T03 Association nominale | Réussi |
| T04 QR expiré ou code différent | Réussi : QR expiré, QR étranger, refus sur le PC, empreinte annulée |
| T05 Connexion après redémarrage | Réussi en VM et sur le PC principal (Wi-Fi Public) : connexion après déconnexion avec « Hello uniquement », déverrouillage après redémarrage, Wi-Fi disponible dès l'écran de verrouillage |
| T06 Sortie de veille | Réussi sur le PC principal (veille moderne) : session ouverte par une nouvelle demande ; résultat Windows non renvoyé (A4) |
| T07 Biométrie réussie | Réussi : une autorisation et un résultat Windows par demande |
| T08 Biométrie refusée ou annulée | Réussi : rien ne s'ouvre |
| T09 Téléphone absent | Réussi : PIN et mot de passe toujours disponibles |
| T10 Réseau coupé | Réussi : l'ancienne preuve est refusée (`EXPIRED`), une nouvelle empreinte est nécessaire |
| T11 Mot de passe changé | Windows accepte encore l'ancien mot de passe sur ce PC, donc pas de refus ; `provision` met à jour (limite Windows, documentée) |
| T12 Empreinte ajoutée | Réussi : clé invalidée, réassociation nécessaire, une seule entrée conservée |
| T13 Révocation, téléphone hors ligne | Réussi |
| T14 Changement de compte | Réussi après correction de A1 |
| T15 Plantage | Plantage du service réussi (PIN disponible, reprise sans ancienne demande). Plantage de la tuile dans LogonUI non provoqué |
| T16 Mise à jour puis désinstallation | Réussi, annulation sur erreur comprise |
| T17 Rejeu | Réussi (tests automatiques et T10) |
| T18 SID, nonce ou contexte modifiés | Réussi (tests automatiques : champ modifié, autre association, nouvel écran) |
| T19 Usurpation | Réussi : épinglage (tests), QR étranger refusé, preuve sans certificat client refusée |
| T20 Utilisateur standard | Réussi : aucun accès aux secrets ni aux canaux, dossiers pré-créés et liens refusés à l'installation |
| T21 Deux preuves simultanées | Couvert par conception (consommation sous verrou, puis rejeu refusé) ; pas d'essai réellement simultané |
| T22 Messages malformés ou trop volumineux | Réussi (script de robustesse) |
| T23 Changement d'heure | Non vérifié en VM (VirtualBox remet l'heure) ; couvert par les tests automatiques (horloge monotone) |
| T24 Historique, diagnostic, effacement | Réussi ; aucun secret dans les journaux : sur les 59 appels de journalisation, seuls 8 écrivent du texte (chemins d'adresse, noms de téléphone, codes de refus), relus le 15/10/2026 |
| T25 Charge et quotas | Réussi : 3 demandes par minute, 60 s de charge sans redémarrage du service, PIN fonctionnel |
| T26 Paquet altéré | Réussi : refusé avant toute modification |

## Performance

| Critère | Mesure | Cible |
|---|---|---|
| Preuve acceptée → résultat Windows | médiane 300 ms, maximum 1 005 ms | 3 s au 95e percentile |
| Taux de succès | 7 sur 7 | 98 sur 100 |
| Processeur au repos (10 min) | 0,005 % | < 1 % |
| Mémoire du service | 2 Mo | < 100 Mo |

## Anomalies

| N° | Gravité | Description | État |
|---|---|---|---|
| S1 | Majeure | Un compte standard présent avant l'installation pouvait préparer `ProgramData\PhoneUnlock` ou le dossier de journal de la tuile pour y glisser une fausse association ou faire écrire SYSTEM ailleurs | Corrigée et vérifiée (T20) |
| A1 | Mineure | La demande lancée depuis la tuile n'était pas annulée quand on choisissait un autre compte | Corrigée et vérifiée (T14) |
| A2 | Mineure | Après `history --clear`, `status` indique « depuis le démarrage du service » alors que c'est depuis l'effacement | Documentée |
| A3 | Mineure | Le test automatique `SessionExpiresAfterTwoMinutes` porte encore l'ancien nom alors que la session dure 5 minutes (D9) | Ouverte, sans effet sur le produit |
| A4 | Mineure | Après une veille moderne, Windows ouvre la session mais ne renvoie pas le résultat à la tuile, et le service ne reçoit aucun avis de session : pas de « Session ouverte » dans l'historique, et l'application attend 10 s avant de conclure | Ouverte, limite de Windows ; sans effet sur la sécurité ni sur l'ouverture |

Aucune anomalie critique ou majeure n'est ouverte.

## Écarts et limites

- **Téléphones** : seul le Pixel 5 (Android 14, Google) a été testé ; le cahier demandait Android 11, 13 et une version récente sur deux fabricants.
- **Performance** : 7 essais au lieu de 100 ; affichage de la demande sur le téléphone non chronométré.
- **BitLocker** non testé (désactivé dans la VM à cause des instantanés).
- **Restauration du système** : introuvable depuis l'environnement de récupération, lente et incomplète (`install.md`, section 8.4) ; le retrait de la tuile depuis l'environnement de récupération est la méthode de secours principale.
- **T11** : Windows accepte encore l'ancien mot de passe après un changement ; relancer `provision` après tout changement fait par sécurité.

## PC principal (L6-4)

Installé le 15/10/2026 par l'utilisatrice, après acceptation écrite du risque principal (D13), vérification de sa clé de récupération BitLocker et création d'un point de restauration. Vérifié sur place : déverrouillage depuis l'application et depuis la tuile, PIN, connexion après déconnexion avec « Hello uniquement », redémarrage, veille moderne (A4). BitLocker est actif sur ce PC, sans effet constaté.

Restent facultatifs : « Hello uniquement » sans Internet (box coupée) et changement d'heure (T23).
