#!/bin/bash

openssl_key()
{
	local KEY=$X509/keys/$1.pem
	local BIT=$2
	echo "  - creating $BIT-bit key in $KEY"
	openssl genrsa -passout pass: -out $KEY $BIT >/dev/null 2>&1
}

openssl_csr()
{
	local STEM=$1
	local SUBJ=$2

	local KEY=$X509/keys/$STEM.pem
	local CSR=$X509/csrs/$STEM.pem
	echo "  - creating CSR in $CSR using key $KEY"
	openssl req -new -nodes -key $KEY -out $CSR -subj "$SUBJ" >/dev/null 2>&1
}

openssl_cert()
{
	local STEM=$1
	local DAYS=$2

	local CSR=$X509/csrs/$STEM.pem
	local CERT=$X509/certs/$STEM.pem
	echo "   - signing $CSR in $CERT"
	openssl x509 -CA $X509/ca/cert.pem -CAkey $X509/ca/key.pem -CAserial $X509/ca/serial \
		-req -days $DAYS -outform PEM -out $CERT -inform PEM -in $CSR >/dev/null 2>&1
}

openssl_fp()
{
	local STEM=$1

	local CERT=$X509/certs/$STEM.pem
	local FPRINT=$(openssl x509 -noout -in $CERT -fingerprint | sed -e 's/.*=//' | dd conv=lcase 2>/dev/null)
	local DEFINE=$(echo "FPRINT_${STEM}_CERT" | sed -e 's/-/_/' | dd conv=ucase 2>/dev/null)

	echo "#define $DEFINE \"$FPRINT\""
}

task "Setting up X509 CA"
X509=$TEST_UNIT_DATA/x509

rm -rf $X509; mkdir -p $X509
mkdir -p $X509/{keys,csrs,certs,ca}
touch $X509/ca/index.txt
echo "123455" > $X509/ca/serial

export OPENSSL_CONF=$X509/ca.conf
cat > $X509/ca.conf <<EOF
[ req ]
default_bits           = 2048
default_keyfile        = $X509/ca/key.pem
default_md             = sha1

prompt                 = no
distinguished_name     = root_ca_dn
x509_extensions        = root_ca_ext

[ root_ca_dn ]
countryName            = US
stateOrProvinceName    = Illinois
localityName           = Peoria
organizationName       = Clockwork Root CA
organizationalUnitName = Policy Master
commonName             = cfm.niftylogic.net

[ root_ca_ext ]
basicConstraints       = CA:true
EOF

echo "  - creating self-signed CA cert in $X509/ca/cert.pem"
openssl req -x509 -nodes -newkey rsa:2048 -out $X509/ca/cert.pem -outform PEM -days 365 >/dev/null 2>&1

unset OPENSSL_CONF

SUBJECT="/C=US/ST=Illinois/L=Peoria"

task "Generating keys for strength tests"
openssl_key rsa128   128
openssl_key rsa1024  1024
openssl_key rsa2048  2048

task "Generating a test 2048-bit key for signing"
openssl_key test 2048

task "Generating a test Certificate Signing Request"
openssl_key  csr 2048
openssl_csr  csr "$SUBJECT/O=NiftyLogic/OU=R&D/OU=CWA/CN=csr.niftylogic.net"

task "Generating another test Certificate Signing Request"
openssl_key  sign-me 2048
openssl_csr  sign-me "$SUBJECT/O=Signing Test/OU=CWA/CN=sign.niftylogic.net"

task "Generating a test Certificate (signed)"
openssl_key  test 2048
openssl_csr  test "$SUBJECT/O=NiftyLogic/OU=CWA/CN=signed.niftylogic.net"
openssl_cert test 365
openssl_fp   test >> $TEST_DEFS_H

task "Generating a test Certificate (signed) for revocation tests"
openssl_key  revoke-me 2048
openssl_csr  revoke-me "$SUBJECT/O=NiftyLogic/OU=R&D/OU=CWA/CN=REVOKE.rd.niftylogic.net"
openssl_cert revoke-me 365
openssl_fp   revoke-me >> $TEST_DEFS_H
