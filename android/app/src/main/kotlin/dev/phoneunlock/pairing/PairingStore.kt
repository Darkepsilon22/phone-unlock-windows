package dev.phoneunlock.pairing

import android.util.AtomicFile
import java.io.File
import java.io.FileNotFoundException

class PairingStore(directory: File) {

    private val file = AtomicFile(File(directory, FILE_NAME))

    fun load(): PairingRecord? = try {
        PairingRecord.decode(file.readFully())
    } catch (absent: FileNotFoundException) {
        null
    }

    fun save(record: PairingRecord) {
        val stream = file.startWrite()
        try {
            stream.write(record.encode())
            file.finishWrite(stream)
        } catch (failure: Exception) {
            file.failWrite(stream)
            throw failure
        }
    }

    private companion object {
        const val FILE_NAME = "pairing.cbor"
    }
}
