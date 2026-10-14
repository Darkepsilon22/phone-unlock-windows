# Installation, mise à jour et récupération

Ce guide s'adresse à la personne qui installe PhoneUnlock sur son propre PC. Les commandes Windows se tapent dans **Windows PowerShell lancé en administrateur** (menu Démarrer, clic droit sur « Windows PowerShell », « Exécuter en tant qu'administrateur »), sauf mention contraire.

Personne n'a besoin de connaître ton mot de passe, ton PIN ou ton empreinte : tu les tapes toi-même quand une commande les demande.

## 1. Avant d'installer

| Condition | Comment la vérifier ou l'obtenir |
|---|---|
| Windows 11 x64 | L'installeur le contrôle |
| Compte Microsoft avec un mot de passe | Le mot de passe doit avoir servi **une fois en ligne** sur ce PC pour que l'ouverture fonctionne aussi sans réseau : se connecter une fois avec « Options de connexion », puis l'icône de clé |
| « Hello uniquement » désactivé | Paramètres, Comptes, Options de connexion : désactiver « Pour plus de sécurité, autoriser uniquement la connexion Windows Hello pour les comptes Microsoft sur cet appareil ». Sinon Windows refuse la connexion par mot de passe (rapport L1). L'installeur le signale sans rien changer |
| Smart App Control désactivé | Sécurité Windows, Contrôle des applications et du navigateur. S'il est actif, l'installeur s'arrête sans rien modifier, car la tuile n'est pas signée par une autorité reconnue (D11). Ne pas le désactiver pour PhoneUnlock |
| Téléphone Android 11 ou plus | Avec une empreinte de classe forte |
| Même réseau | Même Wi-Fi que le PC, ou point d'accès du téléphone (D3) |
| PIN et mot de passe connus | Ils restent tes moyens de secours |

Sur un **vrai PC**, avant la première installation :

1. Valider d'abord la même version dans la VM de test (`docs/testing.md`).
2. Créer un point de restauration : `Checkpoint-Computer -Description 'Avant PhoneUnlock' -RestorePointType APPLICATION_INSTALL` (la protection du système doit être active sur C:).
3. Si BitLocker est actif, vérifier que la clé de récupération est accessible depuis un autre appareil (compte Microsoft, rubrique Appareils). Elle sera demandée pour toute réparation hors de Windows.

## 2. Préparer les paquets (PC de développement)

```powershell
scripts\build-windows.ps1     # build\package\PhoneUnlock-<version>\ avec SHA256SUMS.txt
scripts\sign-apk.ps1          # build\package\PhoneUnlock-<version>.apk, mot de passe de la clé demandé
```

Les deux scripts affichent les empreintes SHA-256. Quand le paquet passe par une clé USB ou un dossier partagé, comparer `SHA256SUMS.txt` de la copie avec ces empreintes. Deux compilations peuvent porter le même numéro de version : seule l'empreinte les distingue.

La clé de signature de l'APK (`%USERPROFILE%\.phoneunlock\release.jks`) et son mot de passe doivent être sauvegardés hors du PC (voir 8.6).

## 3. Installer

### Sur le PC

```powershell
cd <dossier>\PhoneUnlock-<version>
powershell -ExecutionPolicy Bypass -File .\install.ps1
```

Sur un vrai PC, ajouter `-AllowPhysicalMachine`. Sans cette option, l'installeur refuse toute machine qui n'est pas une VM VirtualBox ; une compilation de test (`-test`) y est toujours refusée.

L'installeur vérifie les empreintes, les droits des dossiers et le port, installe le service puis la tuile, et **annule tout** si une étape échoue. Il refuse de continuer, sans rien modifier, si `ProgramData\PhoneUnlock` a été créé par un autre compte ou est un lien (`docs/security.md`, « Dossiers de données »).

Puis, depuis **ta** session :

```powershell
& "C:\Program Files\PhoneUnlock\PhoneUnlockService.exe" provision
```

Tape le mot de passe du compte Microsoft. Windows le vérifie avant qu'il soit enregistré, chiffré.

### Sur le téléphone

Débogage USB activé, téléphone branché :

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r build\package\PhoneUnlock-<version>.apk
```

Si Play Protect avertit, choisir d'installer quand même ; ne pas le désactiver. Le câble peut ensuite être retiré : l'application parle au PC par le réseau.

### Associer

```powershell
& "C:\Program Files\PhoneUnlock\PhoneUnlockService.exe" pair --endpoint <adresse du PC>:47863
```

L'adresse est affichée par `status`. Dans l'application : « Associer un PC », scanner le QR (valable 5 minutes), vérifier que le code à 6 chiffres est **identique** sur les deux écrans, répondre `o` sur le PC, puis confirmer avec l'empreinte.

### Vérifier

1. Verrouiller (Windows + L), ouvrir l'application, « Déverrouiller », poser le doigt.
2. Verrouiller, choisir la tuile Téléphone, « Demander au téléphone », comparer le code à 4 chiffres, autoriser.
3. Lancer `status` (section 6).

## 4. Mettre à jour

**Windows** : relancer `install.ps1` du nouveau paquet depuis ta session ouverte. Le certificat du PC, la liaison TLS, les associations et le mot de passe sont conservés : le téléphone n'a rien à refaire. `status` affiche ensuite les versions installées.

**Application, même clé de signature** : `adb install -r` du nouvel APK. Les clés et l'association du téléphone sont conservées.

**Application, autre clé** (clé perdue ou passage depuis une version debug) : Android refuse la mise à jour. Il faut, dans cet ordre :

1. `PhoneUnlockService.exe revoke` sur le PC, pour retirer l'ancienne association ;
2. `adb uninstall dev.phoneunlock`, ce qui efface les clés du téléphone ;
3. installer le nouvel APK, puis associer à nouveau.

**Mot de passe du compte Microsoft changé** : relancer `provision`. Tant que ce n'est pas fait, Windows refuse l'ouverture et `status` affiche `WINDOWS_REJECTED`.

## 5. Retirer un téléphone

```powershell
& "C:\Program Files\PhoneUnlock\PhoneUnlockService.exe" phones
& "C:\Program Files\PhoneUnlock\PhoneUnlockService.exe" revoke
```

`revoke` affiche la liste numérotée, demande le numéro puis une confirmation. Le téléphone est refusé immédiatement. En cas de perte ou de vol, le faire au plus vite : l'empreinte reste exigée à chaque demande, mais la révocation supprime tout risque.

## 6. Diagnostic

```powershell
& "C:\Program Files\PhoneUnlock\PhoneUnlockService.exe" status
```

| Ligne | Ce qu'elle indique |
|---|---|
| Service | En cours ou arrêté, type de démarrage |
| Versions | Versions du service et de la tuile installés ; tuile enregistrée ou non |
| Adresses du PC | Adresses à donner à `pair` ; « aucune » signifie pas de réseau local |
| Serveur HTTPS | Requête locale en TLS 1.3, sans secret ; « ne répond pas » si le service est arrêté ou le port occupé |
| Mot de passe enregistré | « non » : lancer `provision` |
| Téléphones associés | Téléphones de ton compte |
| Dernier déverrouillage, dernière erreur | Depuis le démarrage du service seulement |

| Code de la dernière erreur | Signification | Que faire |
|---|---|---|
| `REVOKED` | Téléphone inconnu ou révoqué | Associer à nouveau si c'est ton téléphone |
| `INVALID_PROOF` | Signature refusée | Associer à nouveau ; si cela se répète, révoquer |
| `WINDOWS_REJECTED` | Windows a refusé le mot de passe | Relancer `provision` ; vérifier Hello uniquement |
| `EXPIRED` | Empreinte posée trop tard (30 s depuis l'application, 60 s depuis la tuile) | Recommencer |
| `WRONG_CONTEXT` | L'écran de verrouillage a changé pendant la demande | Recommencer |

Sans droits administrateur, `status` n'affiche que les quatre premières lignes.

Journaux, sans aucun secret :

| Fichier | Contenu |
|---|---|
| `C:\ProgramData\PhoneUnlock\logs\service.log` | Service ; lignes `history` pour chaque association, demande, autorisation, révocation et résultat Windows |
| `C:\ProgramData\PhoneUnlock\tile-logs\cp.log` | Tuile ; codes renvoyés par Windows (`result status=…`) |

Ne jamais désactiver Defender, le pare-feu, BitLocker ou l'authentification Windows pour faire fonctionner PhoneUnlock. Si quelqu'un propose de l'aide, il n'a besoin ni de ton mot de passe, ni de ton PIN, ni de ton empreinte ; une intervention à distance demande ton accord explicite.

## 7. Désinstaller

Depuis le dossier du paquet :

```powershell
powershell -ExecutionPolicy Bypass -File .\uninstall.ps1
```

Puis redémarrer. La tuile, le service, la liaison TLS, la règle de pare-feu, le certificat, les associations et le mot de passe enregistré sont retirés. Seuls les journaux restent, dans `C:\ProgramData\PhoneUnlock` ; ce dossier peut ensuite être supprimé à la main (`cmd /c rmdir /s /q C:\ProgramData\PhoneUnlock`). Sur le téléphone, désinstaller l'application.

## 8. Récupération

### 8.1 Le téléphone ne répond pas, est perdu ou déchargé

Rien à faire pour se connecter : sur l'écran de connexion, « Options de connexion » donne toujours le PIN, le mot de passe et Windows Hello. La tuile Téléphone ne remplace aucune méthode de Windows. Ensuite, si le téléphone est perdu : `revoke` (section 5).

### 8.2 Le service est arrêté ou planté

La tuile affiche « Service PhoneUnlock injoignable » ; le PIN fonctionne normalement (vérifié en VM). Pour le relancer : `Start-Service PhoneUnlockService`, puis `status`.

### 8.3 Retirer seulement la tuile

Si la tuile gêne, sans tout désinstaller :

```powershell
Remove-Item 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{512D55BA-432F-42D6-95EF-8071913597AA}'
```

Elle disparaît au prochain verrouillage ; le service continue de tourner. Pour la remettre : relancer `install.ps1`.

### 8.4 Impossible d'ouvrir une session Windows

Ce cas ne devrait pas se produire, puisque les autres méthodes restent disponibles. S'il arrive :

1. Sur l'écran de connexion, maintenir Maj et choisir Marche/Arrêt, Redémarrer ; puis Dépannage, Options avancées.
2. **Restauration du système**, en choisissant le point « Avant PhoneUnlock » (section 1). C'est la voie la plus simple.
3. Sinon, **Invite de commandes**, pour retirer uniquement la tuile (la lettre du disque Windows peut être différente de C: dans cet environnement) :

   ```
   reg load HKLM\HorsLigne C:\Windows\System32\config\SOFTWARE
   reg delete "HKLM\HorsLigne\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{512D55BA-432F-42D6-95EF-8071913597AA}" /f
   reg unload HKLM\HorsLigne
   ```

   Puis redémarrer et se connecter avec le PIN ou le mot de passe.

Si le disque est chiffré par BitLocker, sa clé de récupération est demandée avant ces étapes. **Ces deux procédures n'ont pas encore été essayées** : elles seront vérifiées dans la VM pendant la recette (lot L6).

### 8.5 Dans la VM de test

Restaurer un instantané pris **VM allumée** : Machine, Fermer, Éteindre la machine ; puis, dans le gestionnaire VirtualBox, Instantanés, choisir l'instantané, Restaurer. Un instantané pris VM éteinte réinitialise le TPM virtuel et invalide le PIN (`docs/testing.md`).

### 8.6 Clé de signature de l'APK perdue

L'application installée continue de fonctionner. Seules ses mises à jour deviennent impossibles : créer une nouvelle clé, puis suivre « Application, autre clé » (section 4).
