package me.magnum.melonds.impl.diagnostics

import android.os.Build
import io.github.muntashirakon.adb.AbsAdbConnectionManager
import org.bouncycastle.asn1.x500.X500Name
import org.bouncycastle.cert.jcajce.JcaX509CertificateConverter
import org.bouncycastle.cert.jcajce.JcaX509v3CertificateBuilder
import org.bouncycastle.operator.jcajce.JcaContentSignerBuilder
import java.math.BigInteger
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.PrivateKey
import java.security.cert.Certificate
import java.util.Date

/**
 * Punica's connection to Android's wireless debugging (owner's decision, 29 October), the way Shizuku makes one:
 * used only for the fixed commands of [DiagnosticCommand], never a shell anyone can type into. Its key pair is made
 * when the process starts using it and lives in memory only, so a pairing lasts until Punica's process ends and is
 * made again after (per session). Each pairing adds a "Punica" entry to Android's paired devices list, which the
 * user can forget there.
 */
class WirelessDebugConnection private constructor() : AbsAdbConnectionManager() {

    private val keyPair: KeyPair = KeyPairGenerator.getInstance("RSA").apply { initialize(2048) }.generateKeyPair()
    private val certificate: Certificate = selfSignedCertificate(keyPair)

    init {
        api = Build.VERSION.SDK_INT
    }

    override fun getPrivateKey(): PrivateKey = keyPair.private

    override fun getCertificate(): Certificate = certificate

    override fun getDeviceName(): String = "Punica"

    companion object {
        val instance: WirelessDebugConnection by lazy { WirelessDebugConnection() }

        // adbd checks the certificate's public key only: a self-signed one valid for a day is enough
        private fun selfSignedCertificate(keyPair: KeyPair): Certificate {
            val name = X500Name("CN=Punica")
            val now = System.currentTimeMillis()
            val builder = JcaX509v3CertificateBuilder(
                name,
                BigInteger.valueOf(now),
                Date(now - 60_000),
                Date(now + 86_400_000),
                name,
                keyPair.public,
            )
            val signer = JcaContentSignerBuilder("SHA256withRSA").build(keyPair.private)
            return JcaX509CertificateConverter().getCertificate(builder.build(signer))
        }
    }
}
