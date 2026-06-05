#include "httplib.h"

#include <iostream>
#include <memory>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/table.h>
#include <string>

const int num_threads = 8;
const int chunck_size = 512 * 1024;
httplib::Server server;

// Signal handler
void handle_signal(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		std::cerr << "\n[INFO] Caught Ctrl+C, stopping server..." << std::endl;
		server.stop(); // Gracefully stop accepting new requests
	}
}

int main(int argc, char* argv[])
{
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <db_path>\n";
		return EXIT_FAILURE;
	}

	// DB storage location
	std::string db_path = argv[1];

	// Register signal handler
	std::signal(SIGINT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	// --- RocksDB options ---
	rocksdb::Options options;
	rocksdb::BlockBasedTableOptions table_options;

	// Disable caching
	table_options.no_block_cache = true;
	table_options.cache_index_and_filter_blocks = false;
	options.table_factory.reset(NewBlockBasedTableFactory(table_options));

	options.create_if_missing = true;

	// Blob options
	options.enable_blob_files = true;
	options.min_blob_size = 4096;
	options.blob_file_size = 1024 * 1024;
	options.enable_blob_garbage_collection = false;

	// Use Direct IO to avoid using Linux page cache
	options.use_direct_reads = true;
	options.use_direct_io_for_flush_and_compaction = true;

	// Disable compaction for stable backend
	options.disable_auto_compactions = true;

	// Write buffer size for big objects (256KB)
	options.write_buffer_size = 512 * 1024 * 1024; // 512MB
	options.max_write_buffer_number = 3;

	// Limit the number of compaction
	options.target_file_size_base = 512 * 1024 * 1024; // 512MB

	// Disable compression to avoid CPU overhead
	options.compression = rocksdb::kNoCompression;

	// === Open DB ===
	rocksdb::DB* ptr;
	rocksdb::Status status = rocksdb::DB::Open(options, db_path, &ptr);
	if (!status.ok()) {
		std::cerr << "[ERRO] Error opening RocksDB: " << status.ToString() << std::endl;
		return 1;
	}
	std::unique_ptr<rocksdb::DB> db{ptr};
	std::cout << "[INFO] RocksDB is ready!" << std::endl;

	// === Start HTTP server ===
	server.new_task_queue = [=] { return new httplib::ThreadPool(num_threads); };
	server.Get(R"(/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
		std::string key = req.matches[1];
		std::string value;

		rocksdb::Status s = db->Get(rocksdb::ReadOptions(), key, &value);
		if (!s.ok()) {
			std::cerr << "[INFO] key " << std::quoted(key) << " Not Found\n";
			res.status = httplib::StatusCode::NotFound_404;
			return;
		}

		res.set_content(value.data(), value.size(), "application/octet-stream");
	});

	std::cout << "[INFO] Server listening on port 9000 with " << num_threads << " worker threads..."
			  << std::endl;

	if (!server.listen("0.0.0.0", 9000)) {
		std::cerr << "[ERROR] Server start-up failure\n";
	}

	return 0;
}