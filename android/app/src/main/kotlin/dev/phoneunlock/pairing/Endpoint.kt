package dev.phoneunlock.pairing

import dev.phoneunlock.encoding.Cbor

data class Endpoint(val host: String, val port: Int) {

    fun toCbor(): Cbor = Cbor.Items(listOf(Cbor.Text(host), Cbor.Integer(port.toLong())))

    companion object {
        private const val MAX_HOST_LENGTH = 253
        private const val MAX_PORT = 65535
        private val HOST_PATTERN = Regex("[A-Za-z0-9.-]+")

        fun fromCbor(value: Cbor): Endpoint {
            val parts = requireNotNull(value as? Cbor.Items) { "Adresse mal formée" }.items
            require(parts.size == 2) { "Adresse mal formée" }
            val host = requireNotNull(parts[0] as? Cbor.Text) { "Hôte attendu" }.value
            val port = requireNotNull(parts[1] as? Cbor.Integer) { "Port attendu" }.value
            require(host.length <= MAX_HOST_LENGTH && HOST_PATTERN.matches(host)) { "Hôte invalide" }
            require(port in 1..MAX_PORT) { "Port invalide" }
            return Endpoint(host, port.toInt())
        }

        fun listFromCbor(value: Cbor?, maxCount: Int): List<Endpoint> {
            val items = requireNotNull(value as? Cbor.Items) { "Liste d'adresses attendue" }.items
            require(items.size in 1..maxCount) { "Nombre d'adresses invalide" }
            return items.map(::fromCbor)
        }
    }
}
