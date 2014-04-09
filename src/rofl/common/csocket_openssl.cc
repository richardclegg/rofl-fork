/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "csocket_openssl.h"

using namespace rofl;

/*static*/std::set<csocket_openssl*> csocket_openssl::openssl_sockets;

//Defaults
std::string const	csocket_openssl::PARAM_DEFAULT_VALUE_SSL_KEY_CA_PATH(".");
std::string const	csocket_openssl::PARAM_DEFAULT_VALUE_SSL_KEY_CA_FILE("ca.pem");
std::string const	csocket_openssl::PARAM_DEFAULT_VALUE_SSL_KEY_CERT("cert.pem");
std::string const	csocket_openssl::PARAM_DEFAULT_VALUE_SSL_KEY_PRIVATE_KEY("key.pem");
std::string const	csocket_openssl::PARAM_DEFAULT_VALUE_SSL_KEY_PRIVATE_KEY_PASSWORD("");

/*static*/cparams
csocket_openssl::get_default_params()
{
	cparams p = rofl::csocket_impl::get_default_params();
	p.add_param(csocket::PARAM_SSL_KEY_CA_PATH);
	p.add_param(csocket::PARAM_SSL_KEY_CA_FILE);
	p.add_param(csocket::PARAM_SSL_KEY_CERT);
	p.add_param(csocket::PARAM_SSL_KEY_PRIVATE_KEY);
	p.add_param(csocket::PARAM_SSL_KEY_PRIVATE_KEY_PASSWORD);
	return p;
}

bool csocket_openssl::ssl_initialized = false;

void
csocket_openssl::openssl_init()
{
	if (ssl_initialized)
		return;

	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_ERR_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	BIO_new_fp(stderr, BIO_NOCLOSE);

	ssl_initialized = true;
}


csocket_openssl::csocket_openssl(
		csocket_owner *owner) :
				csocket(owner, rofl::csocket::SOCKET_TYPE_OPENSSL),
				socket(this),
				ctx(NULL),
				method(NULL),
				ssl(NULL),
				bio(NULL)
{
	openssl_sockets.insert(this);

	logging::debug << "[rofl][csocket][openssl] constructor:" << std::endl << *this;

	socket_flags.set(FLAG_SSL_IDLE);

	csocket_openssl::openssl_init();

	pthread_rwlock_init(&ssl_lock, 0);
}



csocket_openssl::~csocket_openssl()
{
	logging::debug << "[rofl][csocket][openssl] destructor:" << std::endl << *this;
	close();

	openssl_destroy_ssl();

	pthread_rwlock_destroy(&ssl_lock);

	openssl_sockets.erase(this);
}



void
csocket_openssl::openssl_init_ctx()
{
	ctx = SSL_CTX_new(SSLv23_method());

	// certificate
	if (!SSL_CTX_use_certificate_file(ctx, certfile.c_str(), SSL_FILETYPE_PEM)) {
		throw eOpenSSL("[rofl][csocket][openssl][init-ctx] unable to read certfile:"+certfile);
	}

	// private key
	SSL_CTX_set_default_passwd_cb(ctx, &csocket_openssl::openssl_password_callback);
	SSL_CTX_set_default_passwd_cb_userdata(ctx, (void*)this);

	if (!SSL_CTX_use_PrivateKey_file(ctx, keyfile.c_str(), SSL_FILETYPE_PEM)) {
		throw eOpenSSL("[rofl][csocket][openssl][init-ctx] unable to read keyfile:"+keyfile);
	}

	// capath/cafile
	if (!SSL_CTX_load_verify_locations(ctx,
			cafile.empty() ? NULL : cafile.c_str(),
			capath.empty() ? NULL : capath.c_str())) {
		throw eOpenSSL("[rofl][csocket][openssl][init-ctx] unable to load ca locations");
	}


	SSL_CTX_set_verify_depth(ctx, 0);

	// TODO: get random numbers
}



void
csocket_openssl::openssl_destroy_ctx()
{
	if (ctx) {
		SSL_CTX_free(ctx); ctx = NULL;
	}
}



void
csocket_openssl::openssl_init_ssl()
{
	openssl_init_ctx();

	if ((ssl = SSL_new(ctx)) == NULL) {
		throw eOpenSSL("[rofl][csocket][openssl][init-ssl] unable to create new SSL object");
	}

	if ((bio = BIO_new(BIO_s_socket())) == NULL) {
		throw eOpenSSL("[rofl][csocket][openssl][init-ssl] unable to create new BIO object");
	}

	BIO_set_fd(bio, socket.get_sd(), BIO_NOCLOSE);

	SSL_set_bio(ssl, /*rbio*/bio, /*wbio*/bio);

	if (socket_flags.test(FLAG_SSL_CONNECTING)) {
		SSL_set_connect_state(ssl);
	} else
	if (socket_flags.test(FLAG_SSL_ACCEPTING)) {
		SSL_set_accept_state(ssl);
	}
}



void
csocket_openssl::openssl_destroy_ssl()
{
	if (ssl) {
		SSL_free(ssl); ssl = NULL; bio = NULL;
	}

	openssl_destroy_ctx();
}



int
csocket_openssl::openssl_password_callback(char *buf, int size, int rwflag, void *userdata)
{
	csocket_openssl *socket = static_cast<csocket_openssl*>(userdata);
	if ((NULL == socket) ||
			(csocket_openssl::openssl_sockets.find(socket) ==
					csocket_openssl::openssl_sockets.end())) {
		throw eOpenSSL("[rofl][csocket][openssl][password-callback] unable to find socket object");
	}

	// use rwflag?

	if (socket->get_password().empty()) {
		return 0;
	}

	strncpy(buf, socket->get_password().c_str(), size);

	return strlen(buf);
}



void
csocket_openssl::listen(
		cparams const& socket_params)
{
	this->socket_params = socket_params;

	capath 		= socket_params.get_param(PARAM_SSL_KEY_CA_PATH).get_string();
	cafile 		= socket_params.get_param(PARAM_SSL_KEY_CA_FILE).get_string();
	certfile	= socket_params.get_param(PARAM_SSL_KEY_CERT).get_string();
	keyfile		= socket_params.get_param(PARAM_SSL_KEY_PRIVATE_KEY).get_string();
	password	= socket_params.get_param(PARAM_SSL_KEY_PRIVATE_KEY_PASSWORD).get_string();

	socket.listen(socket_params);
}



void
csocket_openssl::accept(
		cparams const& socket_params, int sd)
{
	this->socket_params = socket_params;

	capath 		= socket_params.get_param(PARAM_SSL_KEY_CA_PATH).get_string();
	cafile 		= socket_params.get_param(PARAM_SSL_KEY_CA_FILE).get_string();
	certfile	= socket_params.get_param(PARAM_SSL_KEY_CERT).get_string();
	keyfile		= socket_params.get_param(PARAM_SSL_KEY_PRIVATE_KEY).get_string();
	password	= socket_params.get_param(PARAM_SSL_KEY_PRIVATE_KEY_PASSWORD).get_string();

	socket.accept(socket_params, sd);
}



void
csocket_openssl::handle_accepted(rofl::csocket& socket)
{
	socket_flags.reset(FLAG_SSL_IDLE);
	socket_flags.reset(FLAG_SSL_ESTABLISHED);
	socket_flags.set(FLAG_SSL_ACCEPTING);

	openssl_init_ssl();

	openssl_accept();
}



void
csocket_openssl::handle_accept_refused(rofl::csocket& socket)
{
	socket.close();

	if (socket_owner) socket_owner->handle_accept_refused(*this);
}



void
csocket_openssl::connect(
		cparams const& socket_params)
{
	this->socket_params = socket_params;

	capath 		= socket_params.get_param(PARAM_SSL_KEY_CA_PATH).get_string();
	cafile 		= socket_params.get_param(PARAM_SSL_KEY_CA_FILE).get_string();
	certfile	= socket_params.get_param(PARAM_SSL_KEY_CERT).get_string();
	keyfile		= socket_params.get_param(PARAM_SSL_KEY_PRIVATE_KEY).get_string();
	password	= socket_params.get_param(PARAM_SSL_KEY_PRIVATE_KEY_PASSWORD).get_string();

	socket_flags.set(FLAG_ACTIVE_SOCKET);

	socket.connect(socket_params);
}



void
csocket_openssl::handle_connected(rofl::csocket& socket)
{
	socket_flags.reset(FLAG_SSL_IDLE);
	socket_flags.reset(FLAG_SSL_ESTABLISHED);
	socket_flags.set(FLAG_SSL_CONNECTING);

	openssl_init_ssl();

	openssl_connect();
}



void
csocket_openssl::handle_connect_refused(rofl::csocket& socket)
{
	socket.close();

	if (socket_owner) socket_owner->handle_connect_refused(*this);
}



void
csocket_openssl::handle_listen(rofl::csocket& socket, int newsd)
{
	if (socket_owner) socket_owner->handle_listen(*this, newsd);
}



void
csocket_openssl::handle_closed(rofl::csocket& socket)
{
	if (socket_owner) socket_owner->handle_closed(*this);
}



void
csocket_openssl::handle_read(rofl::csocket& socket)
{
	if (socket_flags.test(FLAG_SSL_IDLE)) {
		return; // do nothing

	} else
	if (socket_flags.test(FLAG_SSL_CONNECTING)) {

		openssl_connect();

	} else
	if (socket_flags.test(FLAG_SSL_ACCEPTING)) {

		openssl_accept();

	} else
	if (socket_flags.test(FLAG_SSL_ESTABLISHED)) {

		if (socket_owner) socket_owner->handle_read(*this); // call socket owner => results in a call to this->recv()

	} else
	if (socket_flags.test(FLAG_SSL_CLOSING)) {

		csocket_openssl::close();
	}
}



ssize_t
csocket_openssl::recv(void* buf, size_t count)
{
	int rc = 0;

	if ((rc = SSL_read(ssl, buf, count)) <= 0) {
		switch (SSL_get_error(ssl, rc)) {
		case SSL_ERROR_WANT_READ: {
			rofl::logging::debug << "[rofl][csocket][openssl][recv] receiving => SSL_ERROR_WANT_READ" << std::endl;
		} return rc;
		case SSL_ERROR_WANT_WRITE: {
			rofl::logging::debug << "[rofl][csocket][openssl][recv] receiving => SSL_ERROR_WANT_WRITE" << std::endl;
		} return rc;
		default:
			openssl_destroy_ssl();
			throw eOpenSSL("[rofl][csocket][openssl][handle-read] SSL_read() failed");
		}
	}

	// if there is more data to read, wake-up socket-owner again
	if (SSL_pending(ssl) > 0) {
		notify(EVENT_RECV_RXQUEUE);
	}

	return rc;
}



void
csocket_openssl::handle_write(rofl::csocket& socket)
{
	if (socket_flags.test(FLAG_SSL_IDLE)) {
		return; // do nothing

	} else
	if (socket_flags.test(FLAG_SSL_ACCEPTING)) {

		openssl_accept();

	} else
	if (socket_flags.test(FLAG_SSL_CONNECTING)) {

		openssl_connect();

	} else
	if (socket_flags.test(FLAG_SSL_ESTABLISHED)) {

		dequeue_packet();
	}
}



void
csocket_openssl::send(cmemory *mem, caddress const& dest)
{
	RwLock lock(&ssl_lock, RwLock::RWLOCK_WRITE);

	txqueue.push_back(mem);

	notify(cevent(EVENT_SEND_TXQUEUE));
}



void
csocket_openssl::handle_event(cevent const& e)
{
	switch (e.get_cmd()) {
	case EVENT_SEND_TXQUEUE: {
		dequeue_packet();
	} break;
	case EVENT_RECV_RXQUEUE: {
		if (socket_owner) socket_owner->handle_read(*this); // call socket owner => results in a call to this->recv()
	} break;
	default:
		csocket::handle_event(e);
	}
}


void
csocket_openssl::dequeue_packet()
{
	RwLock lock(&ssl_lock, RwLock::RWLOCK_WRITE);

	while (not txqueue.empty()) {

		int rc = 0, err_code = 0;

		rofl::cmemory *mem = txqueue.front();

		if ((rc = SSL_write(ssl, mem->somem(), mem->memlen())) < 0) {

			switch (err_code = SSL_get_error(ssl, rc)) {
			case SSL_ERROR_WANT_READ: {
				rofl::logging::debug << "[rofl][csocket][openssl][dequeue] sending => SSL_ERROR_WANT_READ" << std::endl;
				notify(EVENT_SEND_TXQUEUE);
			} return;
			case SSL_ERROR_WANT_WRITE: {
				rofl::logging::debug << "[rofl][csocket][openssl][dequeue] sending => SSL_ERROR_WANT_WRITE" << std::endl;
				notify(EVENT_SEND_TXQUEUE);
			} return;
			default: {
			};
			}

		}

		delete mem; txqueue.pop_front();
	}
}



void
csocket_openssl::reconnect()
{
	if (not socket_flags.test(FLAG_ACTIVE_SOCKET)) {
		throw eInval();
	}
	close();
	connect(socket_params);
}



void
csocket_openssl::close()
{
	int rc = 0;

	logging::info << "[rofl][csocket][openssl] close()" << std::endl;

	if (socket_flags.test(FLAG_SSL_IDLE))
		return;

	socket_flags.reset(FLAG_SSL_ESTABLISHED);
	socket_flags.set(FLAG_SSL_CLOSING);

	if (NULL == ssl)
		return;

	if ((rc = SSL_shutdown(ssl)) < 0) {

		switch (SSL_get_error(ssl, rc)) {
		case SSL_ERROR_WANT_READ: {
			rofl::logging::debug << "[rofl][csocket][openssl][close] closing => SSL_ERROR_WANT_READ" << std::endl;
		} return;
		case SSL_ERROR_WANT_WRITE: {
			rofl::logging::debug << "[rofl][csocket][openssl][close] closing => SSL_ERROR_WANT_WRITE" << std::endl;
		} return;
		default:
			openssl_destroy_ssl();
			socket.close();
			throw eOpenSSL("[rofl][csocket][openssl][close] SSL_shutdown() failed");
		}

	} else
	if (rc == 0) {

		if ((rc = SSL_shutdown(ssl)) < 0) {
			switch (SSL_get_error(ssl, rc)) {
			case SSL_ERROR_WANT_READ: {
				rofl::logging::debug << "[rofl][csocket][openssl][close] closing => SSL_ERROR_WANT_READ" << std::endl;
			} return;
			case SSL_ERROR_WANT_WRITE: {
				rofl::logging::debug << "[rofl][csocket][openssl][close] closing => SSL_ERROR_WANT_WRITE" << std::endl;
			} return;
			default:
				openssl_destroy_ssl();
				socket.close();
				throw eOpenSSL("[rofl][csocket][openssl][close] SSL_shutdown() failed");
			}
		}

		openssl_destroy_ssl();
		socket_flags.reset(FLAG_SSL_CLOSING);
		socket_flags.set(FLAG_SSL_IDLE);
		socket.close();

	} else
	if (rc == 1) {

		openssl_destroy_ssl();
		socket_flags.reset(FLAG_SSL_CLOSING);
		socket_flags.set(FLAG_SSL_IDLE);
		socket.close();
	}
}



void
csocket_openssl::openssl_accept()
{
	int rc = 0, err_code = 0;

	rofl::logging::debug << "[rofl][csocket][openssl][accept] SSL accepting..." << std::endl;

	if ((rc = SSL_accept(ssl)) < 0) {
		switch (err_code = SSL_get_error(ssl, rc)) {
		case SSL_ERROR_WANT_READ: {
			// wait for next data from peer
			rofl::logging::debug << "[rofl][csocket][openssl][accept] accepting => SSL_ERROR_WANT_READ" << std::endl;
		} return;
		case SSL_ERROR_WANT_WRITE: {
			rofl::logging::debug << "[rofl][csocket][openssl][accept] accepting => SSL_ERROR_WANT_WRITE" << std::endl;
		} return;
		case SSL_ERROR_WANT_ACCEPT: {
			rofl::logging::debug << "[rofl][csocket][openssl][accept] accepting => SSL_ERROR_WANT_ACCEPT" << std::endl;
		} return;
		case SSL_ERROR_WANT_CONNECT: {
			rofl::logging::debug << "[rofl][csocket][openssl][accept] accepting => SSL_ERROR_WANT_CONNECT" << std::endl;
		} return;


		case SSL_ERROR_NONE: {
			rofl::logging::debug << "[rofl][csocket][openssl][accept] SSL_accept() failed SSL_ERROR_NONE" << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		case SSL_ERROR_SSL: {
			rofl::logging::debug << "[rofl][csocket][openssl][accept] SSL_accept() failed SSL_ERROR_SSL" << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		case SSL_ERROR_SYSCALL: {
			rofl::logging::debug << "[rofl][csocket][openssl][accept] SSL_accept() failed SSL_ERROR_SYSCALL" << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		case SSL_ERROR_ZERO_RETURN: {
			rofl::logging::debug << "[rofl][csocket][openssl][accept] SSL_accept() failed SSL_ERROR_ZERO_RETURN" << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		default: {
			rofl::logging::debug << "[rofl][csocket][openssl][accept] SSL_accept() failed " << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		}

	} else
	if (rc == 0) {

		rofl::logging::debug << "[rofl][csocket][openssl][accept] SSL_accept() failed " << std::endl;

		ERR_print_errors_fp(stderr);

		openssl_destroy_ssl();
		socket.close();
		socket_flags.reset(FLAG_SSL_ACCEPTING);
		socket_flags.set(FLAG_SSL_IDLE);

		if (socket_owner) socket_owner->handle_accept_refused(*this);

	} else
	if (rc == 1) {

		rofl::logging::debug << "[rofl][csocket][openssl][accept] SSL_accept() succeeded " << std::endl;

		socket_flags.reset(FLAG_SSL_ACCEPTING);
		socket_flags.set(FLAG_SSL_ESTABLISHED);

		if (socket_owner) socket_owner->handle_accepted(*this);
	}
}



void
csocket_openssl::openssl_connect()
{
	int rc = 0, err_code = 0;

	rofl::logging::debug << "[rofl][csocket][openssl][connect] SSL connecting..." << std::endl;

	if ((rc = SSL_connect(ssl)) <= 0) {
		switch (err_code = SSL_get_error(ssl, rc)) {
		case SSL_ERROR_WANT_READ: {
			rofl::logging::debug << "[rofl][csocket][openssl][connect] connecting => SSL_ERROR_WANT_READ" << std::endl;
		} return;
		case SSL_ERROR_WANT_WRITE: {
			rofl::logging::debug << "[rofl][csocket][openssl][connect] connecting => SSL_ERROR_WANT_WRITE" << std::endl;
		} return;
		case SSL_ERROR_WANT_ACCEPT: {
			rofl::logging::debug << "[rofl][csocket][openssl][connect] connecting => SSL_ERROR_WANT_ACCEPT" << std::endl;
		} return;
		case SSL_ERROR_WANT_CONNECT: {
			rofl::logging::debug << "[rofl][csocket][openssl][connect] connecting => SSL_ERROR_WANT_CONNECT" << std::endl;
		} return;


		case SSL_ERROR_NONE: {
			rofl::logging::debug << "[rofl][csocket][openssl][connect] SSL_connect() failed SSL_ERROR_NONE" << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		case SSL_ERROR_SSL: {
			rofl::logging::debug << "[rofl][csocket][openssl][connect] SSL_connect() failed SSL_ERROR_SSL" << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		case SSL_ERROR_SYSCALL: {
			rofl::logging::debug << "[rofl][csocket][openssl][connect] SSL_connect() failed SSL_ERROR_SYSCALL" << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		case SSL_ERROR_ZERO_RETURN: {
			rofl::logging::debug << "[rofl][csocket][openssl][connect] SSL_connect() failed SSL_ERROR_ZERO_RETURN" << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		default: {
			rofl::logging::debug << "[rofl][csocket][openssl][connect] SSL_connect() failed " << std::endl;
			ERR_print_errors_fp(stderr);
		} return;
		}

	} else
	if (rc == 0) {

		rofl::logging::debug << "[rofl][csocket][openssl][connect] SSL_connect() failed " << std::endl;
		ERR_print_errors_fp(stderr);

		openssl_destroy_ssl();
		socket.close();
		socket_flags.reset(FLAG_SSL_CONNECTING);
		socket_flags.set(FLAG_SSL_IDLE);

		if (socket_owner) socket_owner->handle_connect_refused(*this);

	} else
	if (rc == 1) {

		rofl::logging::debug << "[rofl][csocket][openssl][connect] SSL_connect() succeeded " << std::endl;

		socket_flags.reset(FLAG_SSL_CONNECTING);
		socket_flags.set(FLAG_SSL_ESTABLISHED);

		if (socket_owner) socket_owner->handle_connected(*this);
	}
}


