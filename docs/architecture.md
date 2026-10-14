# Architecture

## Vue d'ensemble

```
 Téléphone Android                         PC Windows
┌──────────────────────┐   HTTPS TLS 1.3   ┌──────────────────────────────────────────────┐
│ Application          │   mutuel, CBOR    │ PhoneUnlockService (compte NT SERVICE)       │
│  - Keystore StrongBox│ ────────────────▶ │  network/  http.sys, API /v1                 │
│  - BiometricPrompt   │   port 47863      │  pairing/  association, identité du PC       │
└──────────────────────┘                   │  storage/  mot de passe DPAPI, associations  │
                                           │  ipc/      canaux locaux                     │
                                           └───────┬──────────────────────┬───────────────┘
                                   PhoneUnlock.LogonUi            PhoneUnlock.Admin
                                   (SYSTEM uniquement)            (admin élevé uniquement)
                                           │                              │
                              ┌────────────▼────────────┐    ┌────────────▼────────────┐
                              │ PhoneUnlockProtoCP.dll  │    │ PhoneUnlockService.exe  │
                              │ tuile dans LogonUI      │    │ commandes provision,    │
                              │ (Credential Provider)   │    │ pair, console           │
                              └────────────┬────────────┘    └─────────────────────────┘
                                           │ KERB_INTERACTIVE_UNLOCK_LOGON
                                           ▼
                                 Windows (LSA) décide de l'ouverture
```

## Modules

| Module | Responsabilité | Dépend de |
|---|---|---|
| `windows/shared` | Format du canal local (trames, `PayloadReader`/`Writer`), entrées-sorties superposées, journal, mémoire effacée (`SecureVector`, `SecureString`) | Windows uniquement |
| `windows/credential-provider` | Tuile « Téléphone », écoute du service, connexion automatique, sérialisation des identifiants, retour du résultat Windows | `shared` |
| `windows/service` | Service Windows : coffre du mot de passe, autorisations, association, API HTTPS, commandes d'administration | `shared`, Nayuki QR |
| `android` | Clé d'autorisation, biométrie, signature ; association et déverrouillage à venir | AndroidX |
| `protocol/SPEC.md` | Contrat réseau entre le téléphone et le service | — |

Aucune dépendance circulaire : la tuile et le service ne se connaissent qu'à travers `shared`.

### Organisation du service

| Dossier | Contenu |
|---|---|
| `host/` | Point d'entrée, hôte SCM et mode console, assemblage des composants |
| `ipc/` | `PipeServer` (transport), `LogonUiSession` (dialogue avec la tuile), `AdminRequests` (commandes d'administration), `AuthorizationBroker` (autorisations à usage unique) |
| `network/` | `HttpServer` (`http.sys`, TLS 1.3 exigé, certificat client), `Api` (routes), réponses d'erreur |
| `pairing/` | `PairingManager` (états de l'association), `pairing_protocol` (QR, transcription, code), identité du PC |
| `storage/` | `SecretStore` (DPAPI), `PairingStore`, écritures atomiques |
| `encoding/`, `crypto/` | CBOR déterministe strict, UTF-8/hex/base64url, SHA-256, ECDSA P-256 via CNG |
| `admin/` | Commandes en ligne de commande et affichage du QR |
| `support/` | Groupe de threads, horloge monotone injectable |

## Flux

### Connexion ou déverrouillage

1. LogonUI charge la tuile. Celle-ci crée un `ServiceListener` qui se connecte à `PhoneUnlock.LogonUi`, vérifie que l'autre extrémité est bien le processus du service (PID donné par le gestionnaire de services), puis envoie `Wait` avec les SID affichés.
2. Une autorisation est créée pour un SID. Aujourd'hui, seule la commande de test le fait. Plus tard, ce sera la preuve signée du téléphone.
3. Le service consomme l'autorisation (usage unique, 30 s), lit le mot de passe déchiffré et l'envoie (`Ready`).
4. La tuile appelle `CredentialsChanged`, se désigne crédential par défaut avec connexion automatique, sérialise une seule fois, puis efface le mot de passe.
5. Windows rend son verdict (`ReportResult`), que la tuile renvoie au service (`Result`).

### Association (implémentée côté PC, non validée en VM)

1. `PhoneUnlockService pair` demande au service une session : jeton aléatoire de 256 bits, valable 5 minutes. Le QR contient l'identité du PC et les adresses.
2. Le téléphone appelle `POST /v1/pair` avec son certificat de transport et sa clé d'autorisation. Le service répond avec `pairing_id`, puis les deux côtés calculent le même code à 6 chiffres.
3. L'utilisatrice confirme sur le PC, puis sur le téléphone avec une signature biométrique (`POST /v1/pair/confirm`). Le service vérifie la signature et enregistre l'association.

## Frontières de sécurité

| Frontière | Contrôle |
|---|---|
| Réseau → service | TLS 1.3 obligatoire, certificat client exigé, empreintes épinglées, CBOR strict, 16 Ko maximum |
| Tuile → service | Canal accessible à SYSTEM seulement ; la tuile vérifie l'identité du serveur |
| Administrateur → service | Canal accessible aux administrateurs élevés seulement |
| Service → disque | Dossiers `ProgramData\PhoneUnlock\{secrets,state,logs}` limités à SYSTEM, Administrateurs et `NT SERVICE\PhoneUnlockService` ; mot de passe chiffré par DPAPI dans le contexte du service |
| Service → Windows | Aucune élévation : c'est Windows qui accepte ou refuse les identifiants |

## Choix techniques

Les choix et leurs raisons sont dans [decisions.md](decisions.md) : D4 `http.sys`, D5 TLS 1.3, D6 format de sérialisation, D7 commande de test, D8 exécuteur de tests, D9 durée de la session d'association, D10 demande depuis la tuile.

## Conventions

- C++20, `/W4 /WX /sdl /guard:cf`, CRT statique, RAII.
- Kotlin officiel, versions figées dans `android/gradle/libs.versions.toml`.
- Pas de commentaires dans le code : les noms et la documentation suffisent.
- Fichiers PowerShell en UTF-8 avec BOM ; groupes Windows désignés par SID, car le système est en français.
