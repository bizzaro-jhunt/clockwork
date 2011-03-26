#!/bin/bash

set -e # stop on errors
set -x # show commands as they run

DATAROOT=test/data
SAFE_UID=100
SAFE_GID=100

######################################################

setup_res_file() {
	cp -a $DATAROOT/res_file/ORIG/* $DATAROOT/res_file/

	# remove file created by previous run of test_res_file_remediate_new
	rm -f $DATAROOT/res_file/new_file

	chown $SAFE_UID:$SAFE_GID $DATAROOT/res_file/fstab
	chmod 0644 $DATAROOT/res_file/fstab
	chmod 0660 $DATAROOT/res_file/sudoers
}

setup_res_user() {
	rm -rf /tmp/nonexistent
	rm -rf test/tmp/new_user.home
}

setup_userdb() {
	rm -f test/tmp/passwd.new
	rm -f test/tmp/shadow.new
	rm -f test/tmp/group.new
	rm -f test/tmp/gshadow.new
}

setup_cert() {
	X509=test/data/x509

	rm -rf $X509; mkdir -p $X509
	mkdir -p $X509/keys
	mkdir -p $X509/csrs
	mkdir -p $X509/certs

	mkdir -p $X509/ca
	cat > $X509/ca.conf <<EOF
[ ca ]
default_ca             = prime

[ prime ]
dir                    = $X509/ca
certificate            = \$dir/cacert.pem
database               = \$dir/index.txt
new_certs_dir          = \$dir/certs
private_key            = \$dir/key.pem
serial                 = \$dir/serial

default_crl_days       = 7
default_days           = 365
default_md             = sha1

policy                 = prime_policy
x509_extensions        = certificate_extensions

[ prime_policy ]
commonName             = supplied
stateOrProvinceName    = supplied
countryName            = supplied
emailAddress           = supplied
organizationName       = supplied
organizationalUnitName = optional

[ certificate_extensions ]
basicConstraints       = CA:false

# Self-Signed Root CA
[ req ]
default_bits           = 2048
default_keyfile        = $X509/ca/key.pem
default_md             = sha1

prompt                 = no
distinguished_name     = root_ca_distinguished_name

x509_extensions        = root_ca_extensions

[ root_ca_distinguished_name ]
commonName             = cfm.niftylogic.net
stateOrProvinceName    = Illinois
countryName            = US
emailAddress           = ca@cfm.niftylogic.net
organizationName       = Test CFM Root Certification Authority

[ root_ca_extensions ]
basicConstraints       = CA:true
EOF
	export OPENSSL_CONF=test/data/x509/ca.conf
	rm -f  $X509/ca/key.pem
	rm -f  $X509/ca/cert.pem
	rm -f  $X509/ca/index.txt
	rm -f  $X509/ca/serial
	rm -fr $X509/ca/certs

	touch $X509/ca/index.txt
	echo '01' > $X509/ca/serial

	# Create a new self-signed cert
	openssl req -x509 -nodes -newkey rsa:2048 -out $X509/ca/cert.pem -outform PEM -days 365

	unset OPENSSL_CONF

	# Generate 128-, 1024- and 2048-bit RSA keys
	openssl genrsa -passout pass: -out $X509/keys/rsa128.pem  128
	openssl genrsa -passout pass: -out $X509/keys/rsa1024.pem 1024
	openssl genrsa -passout pass: -out $X509/keys/rsa2048.pem 2048

	# Generate a test key
	openssl genrsa -passout pass: -out $X509/keys/test-key.pem 2048

	# Generate a test CSR
	openssl req -new -nodes -key $X509/keys/test-key.pem -out $X509/csrs/request.pem \
		-subj "/C=US/ST=Illinois/L=Peoria/O=NiftyLogic/CN=test.rd.niftylogic.net"

	# Generate a test cert
}

######################################################

setup_res_file
setup_res_user
setup_userdb
setup_cert

