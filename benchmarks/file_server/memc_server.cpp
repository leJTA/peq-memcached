#include "httplib.h"
// #define XXH_INLINE_ALL
// #include "xxhash.h"
#include <atomic>
#include <fstream>
#include <future>
#include <iostream>
#include <libmemcached/memcached.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

const int num_threads = 8;

struct Memc {
	memcached_st* memc;
	Memc()
	{
		memc = memcached_create(nullptr);
		memcached_server_add(memc, "127.0.0.1", 11211);
	}
	~Memc()
	{
		if (memc) {
			memcached_free(memc);
		}
	}
};

thread_local Memc thread_memc;
std::atomic<unsigned long> misses;
std::atomic<unsigned long> hits;
httplib::Server server;

// Signal handler
void handle_signal(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		std::cerr << "\n[INFO] Caught Ctrl+C, stopping server..." << std::endl;
		server.stop(); // Gracefully stop accepting new requests
	}
}

// std::string get_xxhash_prefix(const std::string& key)
// {
// 	XXH64_hash_t hash = XXH64(key.c_str(), key.length(), 0);
// 	std::stringstream ss;
// 	ss << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << hash;
// 	std::string hash_hex = ss.str();
// 	return hash_hex.substr(0, 2);
// }

int main(int argc, char* argv[])
{
	if (argc != 3) {
		std::cerr << "Usage : " << argv[0] << " <db_host> <db_port>\n";
		return EXIT_FAILURE;
	}
	std::string db_host = argv[1];
	int db_port = std::stoi(argv[2]);

	// Register signal handler
	std::signal(SIGINT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	// Per thread http client for remote DB
	thread_local std::unique_ptr<httplib::Client> client;

	// Create a thread pool for handling requests
	server.new_task_queue = [=] { return new httplib::ThreadPool(num_threads); };

	// GET route for files
	server.Get(R"(/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
		std::string key = req.matches[1];

		memcached_st* memc = thread_memc.memc;

		// Try to get from Memcached
		size_t len;
		uint32_t flags;
		char* val = memcached_get(memc, key.c_str(), key.size(), &len, &flags, NULL);
		if (val) {
			res.set_content(std::string(val, len), "application/octet-stream");
			free(val);
			++hits;
			return;
		}
		++misses;

		// Read file from remote DB
		if (!client)
			client = std::make_unique<httplib::Client>("http://" + db_host + ":" +
													   std::to_string(db_port));
		auto cli_res = client->Get("/" + key);
		if (!cli_res) {
			std::cout << "[ERROR] " << cli_res.error() << "\n";
			res.status = httplib::StatusCode::NotFound_404;
			return;
		}

		// Asynchronous store in cache
		auto f = std::async(std::launch::async, [&memc, &key, &cli_res]() {
			memcached_set(memc, key.c_str(), key.size(), cli_res->body.data(),
						  cli_res->body.length(), 0, 0);
		});

		res.set_content(cli_res->body.data(), cli_res->body.length(), "application/octet-stream");

		f.wait();
	});

	std::cout << "[INFO] Listening on port 8000 with " << num_threads << " worker threads..."
			  << std::endl;

	if (!server.listen("0.0.0.0", 8000)) {
		std::cerr << "[ERROR] Server start-up failure\n";
	}

	std::cout << "[INFO] Shutting down gracefully...\n\n"
			  << "[Stats] Number of requests " << hits + misses << "\n"
			  << "          Hits   " << hits << "\n"
			  << "          Misses " << misses
			  << " (Miss rate = " << 100 * (double)misses / (hits + misses) << "%)\n";

	return 0;
}