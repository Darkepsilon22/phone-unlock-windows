# Spécification du protocole PhoneUnlock v1

Statut : version 1.0 du lot L2, dérivée du cahier des charges v1.1 (chapitres 6, 7 et 10) et du [rapport du jalon L1](../docs/rapport-jalon-L1.md).
Les mots **DOIT**, **NE DOIT PAS** et **PEUT** ont valeur d'exigence.

## 1. Vue d'ensemble

| Élément | Choix |
|---|---|
| Rôles | Le **PC** (service Windows) est serveur. Le **téléphone** (application Android) est client. |
| Transport | HTTPS, TLS 1.3 uniquement, authentification mutuelle par certificats auto-signés épinglés |
| Port | 47863/TCP, configurable |
| Encodage | CBOR déterministe (RFC 8949 §4.2.1), maps à clés entières, type MIME `application/cbor` |
| Signature | ECDSA P-256 avec SHA-256 |
| Version | `protocol_version = 1`, chemins préfixés par `/v1` |
| Taille maximale | 16 384 octets par corps de requête ou de réponse |

Le Credential Provider ne parle jamais au réseau : il échange uniquement avec le service par IPC local (hors de ce document).

## 2. Clés et identités

| Clé | Lieu | Algorithme | Protection | Usage |
|---|---|---|---|---|
| Identité PC | Service Windows (CNG, clé machine, ACL du service) | ECDSA P-256 | non exportable | Certificat serveur TLS |
| Transport téléphone | Android Keystore | ECDSA P-256 | non exportable, sans authentification utilisateur | Certificat client TLS |
| Autorisation téléphone | Android Keystore | ECDSA P-256 | non exportable, `BIOMETRIC_STRONG`, une authentification **par opération**, invalidée à l'ajout d'une empreinte, StrongBox si disponible | Signature des autorisations |

Règles :

- La clé d'autorisation **NE DOIT PAS** servir au TLS, et la clé de transport **NE DOIT PAS** signer d'autorisation.
- Aucune clé du téléphone n'entre dans une sauvegarde Android (`allowBackup=false`, exclusion des règles de sauvegarde).
- Les certificats sont auto-signés, valables 10 ans. La confiance repose sur l'**empreinte épinglée** de la clé publique, jamais sur une autorité de certification, un nom d'hôte ou une adresse IP.
- Empreinte d'une clé : `fp(k) = SHA-256(SubjectPublicKeyInfo DER de k)`.

## 3. Association

### 3.1 QR code

Le PC affiche pendant **120 secondes** un QR contenant la chaîne `phoneunlock:1:` suivie du CBOR suivant encodé en base64url sans remplissage.

```cddl
qr-pairing = {
  1: 1,                 ; protocol_version
  2: bstr .size 16,     ; pc_id
  3: bstr .size 32,     ; fp(identité PC)
  4: bstr .size 32,     ; pairing_token, CSPRNG, usage unique
  5: [+ endpoint],      ; adresses du PC sur l'interface retenue
}
endpoint = [host: tstr, port: uint]
```

Le QR **NE DOIT PAS** contenir le nom du compte, le SID ni aucun secret Windows.

### 3.2 Échange

1. Le téléphone génère sa clé de transport et sa clé d'autorisation.
2. Il ouvre une connexion TLS vers un `endpoint`, présente son certificat de transport et **DOIT** vérifier que `fp(certificat serveur) = qr-pairing[3]`. Sinon il abandonne sans rien envoyer.
3. `POST /v1/pair` :

```cddl
pair-request = {
  1: bstr .size 32,          ; pairing_token
  2: bstr,                   ; SubjectPublicKeyInfo DER de la clé d'autorisation
  3: tstr .size (1..64),     ; nom du téléphone
  ? 4: [+ bstr],             ; chaîne d'attestation Keystore (certificats X.509 DER)
}

pair-challenge = {
  1: bstr .size 16,          ; pairing_id
  2: tstr .size (1..64),     ; nom du PC
  3: tstr .size (1..64),     ; nom affichable du compte
}
```

4. Les deux appareils calculent et affichent le **code de comparaison** à six chiffres (§3.3).
5. L'utilisatrice confirme sur le PC, puis sur le téléphone. La confirmation sur le téléphone exige la biométrie et produit une signature avec la clé d'autorisation.
6. `POST /v1/pair/confirm` :

```cddl
pair-confirm = {
  1: bstr .size 16,          ; pairing_id
  2: bstr .size 64,          ; signature r‖s sur pair-tbs (§3.3)
}
```

7. Le PC active l'association seulement si les deux confirmations sont présentes et la signature valide. Il supprime alors le jeton.

### 3.3 Transcription et code de comparaison

```
transcript = SHA-256(
    "PhoneUnlock-v1-pair" ‖ 0x00 ‖
    pc_id ‖ fp(identité PC) ‖ fp(transport téléphone) ‖ fp(autorisation téléphone) ‖
    pairing_token ‖ pairing_id )

code = (uint32 big-endian de transcript[0..4]) mod 1 000 000, sur 6 chiffres avec zéros initiaux
pair-tbs = "PhoneUnlock-v1-pair-confirm" ‖ 0x00 ‖ transcript
```

`fp(transport téléphone)` est calculé par le PC à partir du certificat client TLS, jamais à partir d'un champ du message.

### 3.4 Règles

- Jeton valable 120 s, usage unique, **3 essais** maximum puis invalidation.
- Une association lie : `pairing_id`, `fp(transport)`, clé d'autorisation, `pc_id`, **SID du compte** choisi dans l'assistant.
- Une empreinte inconnue ou différente **NE DOIT JAMAIS** remplacer silencieusement une association existante. Changer de téléphone impose une nouvelle association.

## 4. Demande d'autorisation

### 4.1 Création

Deux déclencheurs créent une demande (décision D1, [docs/decisions.md](../docs/decisions.md)) :

- **Tuile** : bouton « Demander » sur la tuile Windows, `action = 1` ou `2` selon le scénario.
- **Téléphone** : `POST /v1/unlock`, corps vide. Le service crée la demande **uniquement** si l'état courant est `locked` ou `logon_screen` et si la tuile du compte associé est affichée ; sinon `WRONG_CONTEXT`. La réponse est directement l'`authorization-request` du §4.2.

Dans les deux cas, le service génère `request_id` (16 octets) et `nonce` (32 octets) par CSPRNG, enregistre l'instant de création sur son **horloge monotone** et fixe l'expiration à **30 secondes**. Une seule demande active par compte ; une nouvelle demande annule la précédente.

### 4.2 Récupération par le téléphone

`GET /v1/requests/active` : le téléphone est identifié par son certificat client. Réponse `204` si aucune demande, sinon :

```cddl
authorization-request = {
  1: 1,                      ; protocol_version
  2: action,
  3: bstr .size 16,          ; request_id
  4: bstr .size 32,          ; nonce
  5: bstr .size 16,          ; pc_id
  6: bstr .size 16,          ; pairing_id
  7: tstr .size (1..184),    ; account_sid, forme S-1-...
  8: bstr .size 16,          ; context_id, créé par la tuile active
  9: uint,                   ; issued_at, secondes Unix, informatif
  10: uint,                  ; expires_at, secondes Unix, informatif
  11: tstr .size (1..64),    ; nom du PC affiché
  12: tstr .size (1..64),    ; nom du compte affiché
}
action = 1 / 2               ; 1 windows_interactive_logon, 2 windows_unlock
```

Le téléphone **DOIT** vérifier `pc_id` et `pairing_id` par rapport à l'association locale et traiter les champs 11 et 12 comme du texte non fiable (longueur bornée, caractères de contrôle retirés).

### 4.3 Code de correspondance

La tuile et le téléphone affichent le même code à quatre chiffres :

```
match = (uint16 big-endian de SHA-256(CBOR déterministe de authorization-request)[0..2]) mod 10 000
```

### 4.4 Signature

Après « Autoriser » et la biométrie, le téléphone signe :

```cddl
authorization = {
  1..12: les champs de authorization-request, à l'identique,
  13: 1,                     ; decision = allow
}
```

```
authorize-tbs = "PhoneUnlock-v1-authorize" ‖ 0x00 ‖ CBOR déterministe de authorization
```

La clé d'autorisation **DOIT** être utilisée via un `CryptoObject` lié au dialogue biométrique, pour cette seule opération. Un booléen « biométrie réussie » **NE DOIT JAMAIS** remplacer la signature.

`POST /v1/requests/{request_id en hexadécimal}/proof` :

```cddl
authorization-proof = {
  1: bstr .size 16,          ; request_id
  2: bstr .size 64,          ; signature r‖s, entiers de 32 octets big-endian
}
```

Le téléphone n'envoie **que** l'identifiant et la signature. Le PC reconstruit `authorization` à partir de sa propre copie de la demande ; il ne réutilise jamais un encodage fourni par le téléphone.

### 4.5 Validation par le PC

Dans cet ordre, toute erreur étant définitive pour la demande :

1. Le certificat client correspond à une association active (`fp` épinglée).
2. La demande existe, appartient à cette association, est à l'état `présentée`.
3. Le délai monotone de 30 s n'est pas dépassé.
4. La tuile active a toujours le même `context_id` et le même `account_sid`.
5. La signature est valide pour la clé d'autorisation de l'association.
6. La demande passe **atomiquement** à `consommée`, puis seulement le service transmet le secret à la tuile.

Une preuve reçue pour une demande déjà consommée, expirée ou annulée est rejetée sans nouvelle soumission à Windows. La signature n'est pas utilisée comme identifiant anti-rejeu : c'est l'état de la demande qui fait foi.

### 4.6 Résultat

`GET /v1/requests/{request_id}/result`, attente longue jusqu'à 10 s :

```cddl
authorization-result = {
  1: state,
  ? 2: error-code,
}
state = "created" / "presented" / "authorized" / "consumed" /
        "logon_succeeded" / "logon_rejected" / "terminated"
```

Le téléphone n'affiche « Session ouverte » que sur `logon_succeeded`. Sans réponse finale avant la fermeture du canal, il affiche `RESULT_UNKNOWN`.

### 4.7 Soumission par la tuile

Après la consommation (§4.5, étape 6), le service prévient la tuile par IPC. La tuile :

1. appelle `ICredentialProviderEvents::CredentialsChanged` ;
2. désigne son crédential comme crédential par défaut avec connexion automatique dans `GetCredentialCount` ;
3. soumet les identifiants **une seule fois** dans `GetSerialization`, puis retire la connexion automatique ;
4. transmet `ReportResult` au service, qui publie `logon_succeeded` ou `logon_rejected`.

Ce chemin sert aux deux déclencheurs ; en mode tuile, l'utilisatrice n'a pas à recliquer.

## 4bis. État du PC en temps réel

`GET /v1/state?since={version}` : attente longue jusqu'à 25 s, réponse immédiate si la version a changé (décision D2).

```cddl
pc-state = {
  1: uint,                   ; version, incrémentée à chaque changement
  2: session-state,
  3: bool,                   ; unlock_available : POST /v1/unlock accepté maintenant
  4: tstr .size (1..64),     ; nom du PC affiché
}
session-state = "locked" / "logon_screen" / "signed_in" / "unknown"
```

- `locked` : session du compte associé verrouillée ; `logon_screen` : aucune session ouverte ; `signed_in` : session ouverte et déverrouillée.
- L'état ne révèle rien d'autre que ces quatre valeurs pour le compte associé.
- L'application affiche « PC injoignable » si la connexion échoue ou si aucune réponse n'arrive en 30 s.

## 5. États

```
created ─▶ presented ─▶ authorized ─▶ consumed ─▶ logon_succeeded
                                          └────▶ logon_rejected
tout état non final ─▶ terminated (EXPIRED, CANCELLED, REVOKED, WRONG_CONTEXT)
```

Une nouvelle tentative crée toujours une nouvelle demande. Un redémarrage du service efface toutes les demandes.

## 6. Erreurs

Toute réponse d'erreur porte un corps `{ 1: error-code }`.

| Code | HTTP | Cas |
|---|---|---|
| `EXPIRED` | 410 | délai de 30 s dépassé |
| `CANCELLED` | 410 | annulée sur le PC ou remplacée par une nouvelle demande |
| `REVOKED` | 403 | association révoquée ou inconnue |
| `INVALID_PROOF` | 400 | signature invalide ou message malformé |
| `WRONG_CONTEXT` | 409 | tuile, compte ou session changés |
| `WINDOWS_REJECTED` | 200 (dans `authorization-result`) | Windows a refusé les identifiants |
| `RESULT_UNKNOWN` | — | côté téléphone uniquement : canal interrompu |
| `PC_UNREACHABLE` | — | côté téléphone uniquement : connexion impossible |
| `RATE_LIMITED` | 429 | quota dépassé (ajout au cahier, §7) |
| `TOO_LARGE` | 413 | corps supérieur à 16 384 octets (ajout au cahier, §7) |
| `UNSUPPORTED_TLS` | 403 | connexion négociée sans TLS 1.3 (ajout au cahier, §8, décision D5) |

## 7. Limites et quotas

- 3 nouvelles demandes par minute et par compte ; au-delà, `RATE_LIMITED` sans création.
- 3 essais par jeton d'association.
- Délais réseau : 5 s pour l'établissement TLS, 10 s pour l'attente longue.
- Champs texte : 64 caractères au plus après normalisation NFC.
- Le service rejette tout CBOR non déterministe, toute clé inconnue et tout type inattendu.

## 8. Réseau

- Le pare-feu autorise le port 47863/TCP uniquement depuis le sous-réseau local ; le profil Public est conservé. Le serveur HTTPS de Windows (`http.sys`) s'exécute dans le noyau : la règle ne peut donc pas être restreinte au processus du service. Le TLS mutuel épinglé reste la seule preuve d'identité.
- Le service refuse toute requête dont le protocole négocié n'est pas TLS 1.3 (`UNSUPPORTED_TLS`), même si la configuration de Windows autorise TLS 1.2.
- Aucune découverte réseau générale, aucun UPnP, aucun relais Internet.
- Si l'adresse du PC change, l'utilisatrice la ressaisit ou rescanne un QR de reconnexion, qui ne contient pas de jeton d'association.

### 8.1 Sans connexion (décision D3)

| Mode | Fonctionnement |
|---|---|
| Wi-Fi commun sans Internet | Inchangé |
| Point d'accès du téléphone | Le PC enregistre le point d'accès comme réseau connu pour tous les utilisateurs, donc disponible à l'écran de connexion. Le téléphone cherche le service sur son propre sous-réseau de point d'accès (port 47863) et n'accepte que l'identité PC épinglée. |
| Bluetooth Low Energy | Lot ultérieur : mêmes messages CBOR, canal à spécifier |

L'ouverture d'un compte Microsoft sans Internet repose sur les identifiants en cache de Windows : à vérifier en VM avant d'annoncer ce mode.
- Pendant les tests en VM (NAT VirtualBox), une redirection `hôte:47863 → VM:47863` est ajoutée ; elle est retirée après les tests.

## 9. Sécurité : correspondance avec le cahier

| Menace | Mesures du protocole |
|---|---|
| S01 rejeu | nonce, `request_id`, expiration monotone, consommation atomique (§4.5) |
| S02 usurpation | empreintes épinglées, TLS 1.3 mutuel, code de comparaison (§3) |
| S03 téléphone perdu | biométrie par opération, révocation côté PC (`REVOKED`) |
| S04 commande forgée | signature obligatoire, reconstruction côté PC (§4.4) |
| S05 mauvais compte | `account_sid` et `context_id` signés et revérifiés (§4.5) |
| S08 saturation | quotas, taille maximale, une demande active (§7) |
| S10 nouvelle biométrie | clé invalidée à l'enrôlement, nouvelle association obligatoire |
| S12 mauvaise demande | mode tuile : initiation sur le PC et code de correspondance ; mode téléphone : PC réellement verrouillé, demande liée à ce verrouillage, noms signés, historique (§4.1, décision D1) |

## 10. Compatibilité

- Un champ inconnu ou une `protocol_version` différente provoque un rejet explicite.
- Une évolution incompatible change le préfixe `/v1` et les domaines de signature `PhoneUnlock-v1-…`.
