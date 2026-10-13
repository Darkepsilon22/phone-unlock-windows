package dev.phoneunlock.scanner

import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import com.google.zxing.BarcodeFormat
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.MultiFormatReader
import com.google.zxing.PlanarYUVLuminanceSource
import com.google.zxing.ReaderException
import com.google.zxing.common.HybridBinarizer

class QrFrameDecoder(private val onDecoded: (String) -> Unit) : ImageAnalysis.Analyzer {

    private val reader = MultiFormatReader().apply { setHints(HINTS) }

    override fun analyze(image: ImageProxy) {
        image.use { decode(it) }?.let(onDecoded)
    }

    private fun decode(image: ImageProxy): String? {
        val luminance = image.planes.first()
        val data = ByteArray(luminance.buffer.remaining()).also { luminance.buffer.get(it) }
        val source = PlanarYUVLuminanceSource(data, luminance.rowStride, image.height, 0, 0, image.width, image.height, false)
        return try {
            reader.decodeWithState(BinaryBitmap(HybridBinarizer(source))).text
        } catch (notFound: ReaderException) {
            null
        } finally {
            reader.reset()
        }
    }

    private companion object {
        val HINTS = mapOf(
            DecodeHintType.POSSIBLE_FORMATS to listOf(BarcodeFormat.QR_CODE),
            DecodeHintType.TRY_HARDER to true,
            DecodeHintType.ALSO_INVERTED to true,
        )
    }
}
