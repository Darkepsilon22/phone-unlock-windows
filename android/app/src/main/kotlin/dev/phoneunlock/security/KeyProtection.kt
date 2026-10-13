package dev.phoneunlock.security

enum class KeyProtection(val label: String) {
    STRONGBOX("StrongBox (puce de sécurité dédiée)"),
    TRUSTED_ENVIRONMENT("Environnement d'exécution sécurisé (TEE)"),
    SOFTWARE("Logiciel uniquement"),
    UNKNOWN("Inconnue"),
}
