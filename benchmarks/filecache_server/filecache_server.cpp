#include "httplib.h"
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <libmemcached/memcached.h>

const std::string DATA_DIR = "/tmp/3q-items-data";
const int NUM_THREADS = 4;

struct Memc {
	memcached_st* memc;
	Memc() {
		memc = memcached_create(nullptr);
		memcached_server_add(memc, "127.0.0.1", 11211);
	}
	~Memc() {
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
void handle_signal(int sig) {
	if (sig == SIGINT || sig == SIGTERM) {
		std::cerr << "\n[Signal] Caught Ctrl+C, stopping server..." << std::endl;
		server.stop(); // Gracefully stop accepting new requests
	}
}

int main()
{
	// Register signal handler
	std::signal(SIGINT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	// Create a thread pool for handling requests
	server.new_task_queue = [=] { return new httplib::ThreadPool(NUM_THREADS); };

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

		// Read file from disk
		std::ifstream file(DATA_DIR + "/" + key, std::ios::binary);
		if (!file) {
			res.status = 404;
			return;
		}
		++misses;

		std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
										 std::istreambuf_iterator<char>());

		res.set_content(buffer.data(), buffer.size(), "application/octet-stream");

		// Store in cache
		memcached_set(memc, key.c_str(), key.size(), buffer.data(), buffer.size(), 0, 0);
	});

	std::cout << "[Server] Listening on port 8080 with " << NUM_THREADS << " worker threads..." 
				 << std::endl;

	if (!server.listen("0.0.0.0", 8000)) {
		std::cerr << "Server start-up failure\n";
	}
	
	std::cout << "[Server] Shutting down gracefully...\n\n";
	std::cout << "[Stats] Hits   " << hits << "\n";
	std::cout << "[Stats] Misses " << misses << " (Miss rate = " 
				 << 100 * (double)misses / (hits + misses) << "%)\n";

	return 0;
}
