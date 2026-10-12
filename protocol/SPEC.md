# Spécification du protocole (brouillon v0)

Dérivée du cahier des charges v1.1, chapitre 7. À compléter au lot L2.

## Transport

- HTTPS, TLS 1.3, authentification mutuelle, identités épinglées.
- Port proposé : 47863/TCP (à confirmer après le jalon L1).
- Message maximum : 16 Ko. Une demande active par compte. 3 nouvelles demandes par minute et par compte.

## Association

1. Le PC affiche un QR (valable 120 s) : `pc_id`, empreinte de la clé publique PC, jeton aléatoire 256 bits à usage unique.
2. Le téléphone scanne, génère ses clés, ouvre le canal TLS.
3. Code à six chiffres dérivé de la transcription authentifiée, comparé sur les deux écrans.
4. Confirmation sur les deux appareils : liaison clés publiques, PC, `account_sid`. Jeton supprimé. 3 essais maximum par jeton.

## Demande de déverrouillage (contenu signé)

| Champ | Règle |
|---|---|
| `protocol_version`, `action` | `windows_interactive_logon` ou `windows_unlock` |
| `request_id`, `nonce` | identifiant unique ; nonce 256 bits (CSPRNG du PC) |
| `pc_id`, `pairing_id` | PC destinataire et association |
| `account_sid`, `context_id` | compte précis, contexte de la tuile active |
| `issued_at`, `expires_at` | informatifs ; expiration imposée par le PC (30 s, horloge monotone) |
| `decision` | `allow` |

- Signature : ECDSA P-256 / SHA-256, clé Android Keystore non exportable, biométrie forte à chaque opération.
- Encodage : CBOR déterministe (RFC 8949 §4.2) ; préfixe de domaine propre au produit ; représentation de signature unique (à fixer : r‖s 64 octets).
- Interdit : un simple booléen « biométrie réussie ».

## Validation côté PC

Canal, association, clé, signature, compte, contexte actif, expiration. La demande est consommée **atomiquement avant** soumission à Windows, même si Windows refuse.

## États et codes d'erreur

États : créée, présentée, autorisée, consommée, connexion réussie | refusée. Terminaux : annulation, expiration, révocation, changement de tuile.

Codes : `EXPIRED`, `CANCELLED`, `REVOKED`, `INVALID_PROOF`, `WRONG_CONTEXT`, `PC_UNREACHABLE`, `WINDOWS_REJECTED`, `RESULT_UNKNOWN`.

## À décider (L2)

- [ ] Format exact des champs CBOR et des clés de map
- [ ] Dérivation du code à six chiffres
- [ ] Gestion des certificats TLS épinglés (auto-signés, rotation)
- [ ] Lien PC ↔ VM en NAT (redirection de port) pendant les tests
