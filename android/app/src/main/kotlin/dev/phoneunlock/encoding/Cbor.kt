package dev.phoneunlock.encoding

import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.charset.CharacterCodingException
import java.nio.charset.CodingErrorAction

sealed interface Cbor {
    data class Integer(val value: Long) : Cbor {
        init {
            require(value >= 0) { "Entier négatif non pris en charge" }
        }
    }

    data class Flag(val value: Boolean) : Cbor

    class ByteString(value: ByteArray) : Cbor {
        private val content = value.copyOf()

        val value: ByteArray get() = content.copyOf()

        override fun equals(other: Any?): Boolean = other is ByteString && content.contentEquals(other.content)

        override fun hashCode(): Int = content.contentHashCode()
    }

    data class Text(val value: String) : Cbor

    data class Items(val items: List<Cbor>) : Cbor

    data class Object(val entries: Map<Long, Cbor>) : Cbor {
        init {
            require(entries.keys.all { it >= 0 }) { "Clé négative non prise en charge" }
        }

        operator fun get(key: Long): Cbor? = entries[key]
    }

    companion object {
        fun encode(value: Cbor): ByteArray = ByteArrayOutputStream().also { Encoder(it).write(value) }.toByteArray()

        fun decode(bytes: ByteArray): Cbor {
            val decoder = Decoder(bytes)
            val value = decoder.readValue(depth = 0)
            require(decoder.atEnd) { "Octets en trop après la valeur CBOR" }
            return value
        }
    }
}

private object Major {
    const val UNSIGNED = 0
    const val BYTES = 2
    const val TEXT = 3
    const val ARRAY = 4
    const val MAP = 5
    const val SIMPLE = 7
}

private const val FALSE_VALUE = 20
private const val TRUE_VALUE = 21
private const val ONE_BYTE = 24
private const val TWO_BYTES = 25
private const val FOUR_BYTES = 26
private const val EIGHT_BYTES = 27
private const val MAX_DEPTH = 8
private const val MAX_ITEMS = 64
private const val MAJOR_SHIFT = 5
private const val ADDITIONAL_MASK = 0x1F

private class Encoder(private val out: ByteArrayOutputStream) {

    fun write(value: Cbor) {
        when (value) {
            is Cbor.Integer -> writeHead(Major.UNSIGNED, value.value)
            is Cbor.Flag -> out.write((Major.SIMPLE shl MAJOR_SHIFT) or if (value.value) TRUE_VALUE else FALSE_VALUE)
            is Cbor.ByteString -> writeBytes(Major.BYTES, value.value)
            is Cbor.Text -> writeBytes(Major.TEXT, value.value.toByteArray(Charsets.UTF_8))
            is Cbor.Items -> {
                writeHead(Major.ARRAY, value.items.size.toLong())
                value.items.forEach(::write)
            }
            is Cbor.Object -> {
                writeHead(Major.MAP, value.entries.size.toLong())
                value.entries.toSortedMap().forEach { (key, item) ->
                    writeHead(Major.UNSIGNED, key)
                    write(item)
                }
            }
        }
    }

    private fun writeBytes(major: Int, bytes: ByteArray) {
        writeHead(major, bytes.size.toLong())
        out.write(bytes)
    }

    private fun writeHead(major: Int, argument: Long) {
        val type = major shl MAJOR_SHIFT
        if (argument < ONE_BYTE) {
            out.write(type or argument.toInt())
            return
        }
        val (additional, width) = when {
            argument <= 0xFF -> ONE_BYTE to 1
            argument <= 0xFFFF -> TWO_BYTES to 2
            argument <= 0xFFFF_FFFFL -> FOUR_BYTES to 4
            else -> EIGHT_BYTES to 8
        }
        out.write(type or additional)
        for (shift in (width - 1) * Byte.SIZE_BITS downTo 0 step Byte.SIZE_BITS) {
            out.write((argument ushr shift).toInt() and 0xFF)
        }
    }
}

private class Decoder(private val input: ByteArray) {
    private var position = 0

    val atEnd: Boolean get() = position == input.size

    fun readValue(depth: Int): Cbor {
        require(depth <= MAX_DEPTH) { "Imbrication CBOR trop profonde" }
        val (major, argument) = readHead()
        return when (major) {
            Major.UNSIGNED -> Cbor.Integer(argument)
            Major.BYTES -> Cbor.ByteString(take(argument))
            Major.TEXT -> Cbor.Text(strictUtf8(take(argument)))
            Major.ARRAY -> Cbor.Items(List(checkedCount(argument)) { readValue(depth + 1) })
            Major.MAP -> readObject(checkedCount(argument), depth)
            else -> Cbor.Flag(argument == TRUE_VALUE.toLong())
        }
    }

    private fun readObject(count: Int, depth: Int): Cbor.Object {
        val entries = LinkedHashMap<Long, Cbor>()
        var previous = -1L
        repeat(count) {
            val (major, key) = readHead()
            require(major == Major.UNSIGNED && key > previous) { "Clés CBOR non entières, non triées ou dupliquées" }
            entries[key] = readValue(depth + 1)
            previous = key
        }
        return Cbor.Object(entries)
    }

    private fun readHead(): Pair<Int, Long> {
        val initial = next()
        val major = initial ushr MAJOR_SHIFT
        val additional = initial and ADDITIONAL_MASK
        if (major == Major.SIMPLE) {
            require(additional == FALSE_VALUE || additional == TRUE_VALUE) { "Valeur simple CBOR non prise en charge" }
            return major to additional.toLong()
        }
        require(major in setOf(Major.UNSIGNED, Major.BYTES, Major.TEXT, Major.ARRAY, Major.MAP)) {
            "Type CBOR non pris en charge"
        }
        if (additional < ONE_BYTE) {
            return major to additional.toLong()
        }
        val (width, minimum) = when (additional) {
            ONE_BYTE -> 1 to ONE_BYTE.toLong()
            TWO_BYTES -> 2 to 0x100L
            FOUR_BYTES -> 4 to 0x1_0000L
            EIGHT_BYTES -> 8 to 0x1_0000_0000L
            else -> throw IllegalArgumentException("Longueur CBOR indéfinie ou réservée")
        }
        var value = 0L
        repeat(width) { value = (value shl Byte.SIZE_BITS) or next().toLong() }
        require(value >= minimum) { "Entier CBOR non minimal ou hors limites" }
        return major to value
    }

    private fun checkedCount(argument: Long): Int {
        require(argument <= MAX_ITEMS) { "Trop d'éléments CBOR" }
        return argument.toInt()
    }

    private fun take(length: Long): ByteArray {
        require(length <= input.size - position) { "Données CBOR tronquées" }
        return input.copyOfRange(position, position + length.toInt()).also { position += length.toInt() }
    }

    private fun next(): Int {
        require(position < input.size) { "Données CBOR tronquées" }
        return input[position++].toInt() and 0xFF
    }

    private fun strictUtf8(bytes: ByteArray): String = try {
        Charsets.UTF_8.newDecoder()
            .onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT)
            .decode(ByteBuffer.wrap(bytes))
            .toString()
    } catch (invalid: CharacterCodingException) {
        throw IllegalArgumentException("Texte CBOR invalide", invalid)
    }
}
