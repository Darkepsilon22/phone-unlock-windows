package dev.phoneunlock.unlock

import dev.phoneunlock.encoding.Cbor

enum class SessionState(val wire: String) {
    LOCKED("locked"),
    LOGON_SCREEN("logon_screen"),
    SIGNED_IN("signed_in"),
    UNKNOWN("unknown"),
}

data class PcStatus(val version: Long, val session: SessionState, val unlockAvailable: Boolean, val pcName: String) {

    companion object {
        private const val VERSION_FIELD = 1L
        private const val SESSION_FIELD = 2L
        private const val AVAILABLE_FIELD = 3L
        private const val PC_NAME_FIELD = 4L
        private const val MAX_NAME_LENGTH = 64
        private val FIELDS = setOf(VERSION_FIELD, SESSION_FIELD, AVAILABLE_FIELD, PC_NAME_FIELD)

        fun parse(body: Cbor.Object?): PcStatus {
            val fields = requireNotNull(body) { "État absent" }.entries
            require(fields.keys == FIELDS) { "Champs de l'état inattendus" }
            val version = requireNotNull(fields[VERSION_FIELD] as? Cbor.Integer) { "Version attendue" }.value
            val session = requireNotNull(fields[SESSION_FIELD] as? Cbor.Text) { "État attendu" }.value
            val available = requireNotNull(fields[AVAILABLE_FIELD] as? Cbor.Flag) { "Disponibilité attendue" }.value
            val name = requireNotNull(fields[PC_NAME_FIELD] as? Cbor.Text) { "Nom attendu" }.value
            return PcStatus(
                version = version,
                session = SessionState.entries.firstOrNull { it.wire == session } ?: SessionState.UNKNOWN,
                unlockAvailable = available,
                pcName = name.filterNot(Char::isISOControl).trim().take(MAX_NAME_LENGTH),
            )
        }
    }
}
