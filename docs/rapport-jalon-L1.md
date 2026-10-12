# Rapport du jalon L1 : preuve de faisabilité Windows

Date : 12 octobre 2026
Statut : **validé sous condition** (Hello uniquement désactivé)

## Objectif

Démontrer qu'un Credential Provider tiers peut ouvrir une session Windows 11 pour un **compte Microsoft**, en soumettant le mot de passe du compte, avec la méthode Windows habituelle toujours disponible (cahier des charges v1.1, chapitre 2).

## Environnement

| Élément | Valeur |
|---|---|
| Hôte de test | VM VirtualBox 7.2.14 `win11-test`, sans Hyper-V |
| Système | Windows 11 Famille 25H2 x64 (build 26200) |
| Compte | Compte Microsoft de test, avec mot de passe et PIN |
| Protections | Defender actif, Secure Boot, TPM 2.0 virtuel, Smart App Control désactivé (comme le PC cible) |
| BitLocker | Désactivé dans la VM uniquement (voir Limites) |
| Prototype | `windows/credential-provider`, DLL `PhoneUnlockProtoCP.dll`, CLSID `{512D55BA-432F-42D6-95EF-8071913597AA}` |
| Sérialisation | `CredPackAuthenticationBufferW` (`CRED_PACK_PROTECTED_CREDENTIALS \| CRED_PACK_ID_PROVIDER_CREDENTIALS`), paquet Negotiate |

## Résultats

| Heure | Condition | Scénario | Statut | Sous-statut | Résultat |
|---|---|---|---|---|---|
| 10:31:05 | Hello uniquement activé | Connexion | `0xC000006D` | `0xC000015B` | Refus : type de connexion non accordé |
| 11:06:36 | Hello uniquement activé | Connexion | `0xC000006D` | `0xC000015B` | Refus : type de connexion non accordé |
| 11:19:04 | Hello uniquement désactivé, Verr Maj active | Connexion | `0xC000006D` | `0x00000000` | Refus : mot de passe incorrect |
| 11:25:09 | Hello uniquement désactivé | Connexion | `0x00000000` | `0x00000000` | **Session ouverte** |
| 11:31:11 | Hello uniquement désactivé | Déverrouillage (Win + L) | `0x00000000` | `0x00000000` | **Session déverrouillée** |
| 11:32:28 | Hello uniquement désactivé | Après redémarrage | `0x00000000` | `0x00000000` | **Session ouverte** |

Source : `C:\ProgramData\PhoneUnlockProto\cp.log` dans la VM (aucun secret ni nom de compte journalisé).

## Constats

1. **Compte Microsoft** : Windows accepte un mot de passe soumis par un fournisseur tiers pour un compte Microsoft. La piste « Credential Provider et secret local protégé » est techniquement viable.
2. **Hello uniquement bloque la méthode** : quand `DevicePasswordLessBuildVersion = 2`, l'autorité de sécurité refuse toute connexion par mot de passe pour le compte Microsoft (`STATUS_LOGON_TYPE_NOT_GRANTED`), y compris depuis un fournisseur tiers. Il ne s'agit pas d'un simple masquage de la tuile mot de passe.
3. **Réactivation apparente** : l'écran Paramètres réaffiche l'option activée juste après sa désactivation, alors que le registre indiquait `0`. La cause n'est pas établie. Après un redémarrage avec la valeur `0`, la tuile mot de passe de Windows apparaît.
4. **Scénario d'usage** : Windows 11 25H2 transmet `CPUS_LOGON` aussi pour le déverrouillage après Win + L. `CPUS_UNLOCK_WORKSTATION` n'a jamais été observé.
5. **Secours** : le PIN et la tuile mot de passe de Windows sont restés disponibles pendant tous les essais. Aucun plantage de LogonUI.

## Limites

- Le mot de passe a été saisi à la main dans la tuile. Le stockage chiffré par le service, le téléphone et le réseau restent à réaliser (lots L2 à L4).
- BitLocker a été désactivé dans la VM : les instantanés VirtualBox ne conservent pas correctement le TPM virtuel, ce qui a rendu une première VM inutilisable. Le comportement avec BitLocker actif reste à vérifier sur le PC cible.
- Le réseau avant ouverture de session n'a pas encore été testé (jalon réseau, lot L2).
- Un seul essai par scénario : les objectifs de performance (100 essais, 95e percentile) restent à mesurer.

## Décision requise

Sur le PC de l'utilisatrice, `DevicePasswordLessBuildVersion = 2` (Hello uniquement activé). D'après ce jalon, la méthode ne fonctionnera que si l'utilisatrice **désactive elle-même** Hello uniquement et si son compte Microsoft possède un mot de passe. Le PIN et Windows Hello restent utilisables. Le produit ne modifiera jamais ce réglage automatiquement.

| Choix | Conséquence |
|---|---|
| Désactiver Hello uniquement sur le PC | Poursuite de l'architecture actuelle (L2 à L6) |
| Conserver Hello uniquement | Jalon refusé pour ce PC ; étude « authentification sans secret réutilisable » ou alternative de repli (lecteur biométrique Windows) |
