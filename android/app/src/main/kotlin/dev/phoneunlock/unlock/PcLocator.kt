package dev.phoneunlock.unlock

import dev.phoneunlock.pairing.Endpoint
import java.io.IOException
import java.net.Inet4Address
import java.net.InetSocketAddress
import java.net.NetworkInterface
import java.net.Socket
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.withContext

class PcLocator(private val probe: (Endpoint) -> Boolean) {

    suspend fun locate(known: List<Endpoint>): Endpoint? = withContext(Dispatchers.IO) {
        known.firstOrNull(probe) ?: scanLocalNetworks(known.firstOrNull()?.port ?: DEFAULT_PORT)
    }

    private suspend fun scanLocalNetworks(port: Int): Endpoint? = coroutineScope {
        val listening = localSubnetHosts()
            .map { host -> async(SCAN_DISPATCHER) { host.takeIf { isPortOpen(it, port) } } }
            .awaitAll()
            .filterNotNull()
        listening.map { Endpoint(it, port) }.firstOrNull(probe)
    }

    private fun isPortOpen(host: String, port: Int): Boolean = try {
        Socket().use { it.connect(InetSocketAddress(host, port), CONNECT_PROBE_MS) }
        true
    } catch (closed: IOException) {
        false
    }

    companion object {
        private const val DEFAULT_PORT = 47863
        private const val CONNECT_PROBE_MS = 400
        private const val PARALLEL_PROBES = 64
        private val SCAN_DISPATCHER = Dispatchers.IO.limitedParallelism(PARALLEL_PROBES)

        fun localSubnetHosts(): List<String> =
            NetworkInterface.getNetworkInterfaces().toList()
                .filter { it.isUp && !it.isLoopback }
                .flatMap { it.interfaceAddresses }
                .mapNotNull { address -> (address.address as? Inet4Address)?.let { it.address to address.networkPrefixLength.toInt() } }
                .flatMap { (address, prefix) -> subnetHosts(address, prefix) }
                .distinct()

        fun subnetHosts(address: ByteArray, prefixLength: Int): List<String> {
            if (address.size != IPV4_SIZE || prefixLength !in MIN_PREFIX..MAX_PREFIX) {
                return emptyList()
            }
            val self = address.fold(0L) { value, byte -> (value shl Byte.SIZE_BITS) or (byte.toLong() and 0xFF) }
            val effectivePrefix = maxOf(prefixLength, SCAN_PREFIX)
            val mask = (0xFFFF_FFFFL shl (IPV4_BITS - effectivePrefix)) and 0xFFFF_FFFFL
            val network = self and mask
            val broadcast = network or (mask.inv() and 0xFFFF_FFFFL)
            return (network + 1 until broadcast).filter { it != self }.map(::dotted)
        }

        private fun dotted(value: Long): String = (3 downTo 0).joinToString(".") { ((value shr (it * Byte.SIZE_BITS)) and 0xFF).toString() }

        private const val IPV4_SIZE = 4
        private const val IPV4_BITS = 32
        private const val MIN_PREFIX = 8
        private const val MAX_PREFIX = 30
        private const val SCAN_PREFIX = 24
    }
}
