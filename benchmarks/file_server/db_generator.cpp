#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/table.h>

std::string format_key(uint64_t id)
{
	std::ostringstream oss;
	oss << "key_" << std::setw(9) << std::setfill('0') << id;
	return oss.str();
}

int main(int argc, char** argv)
{
	if (argc != 5) {
		std::cerr << "Usage: " << argv[0] << " <input_file> <db_path> <num_blobs> <blob_size>\n";
		return EXIT_FAILURE;
	}

	std::string input_file = argv[1];
	std::string db_path = argv[2];
	uint64_t num_blobs = std::stoull(argv[3]);
	size_t blob_size = std::stoull(argv[4]);

	if (blob_size < 4 * 1024) {
		std::cerr << "[ERROR] Blob size must be >= 4KB\n";
		return EXIT_FAILURE;
	}

	// Read file
	std::ifstream in(input_file, std::ios::binary);
	if (!in) {
		std::cerr << "[ERROR] Unable to read file " << std::quoted(input_file) << "\n";
		return EXIT_FAILURE;
	}

	std::vector<char> buffer(blob_size);
	in.read(buffer.data(), blob_size);

	if ((size_t)in.gcount() != blob_size) {
		std::cerr << "[ERROR] Input file to small\n";
		return EXIT_FAILURE;
	}

	std::cout << "[INFO] Buffer loaded (" << blob_size / 1024.0 << " KB)\n";

	// -----------------------------
	// Configuration RocksDB Blob
	// -----------------------------
	rocksdb::Options options;
	options.create_if_missing = true;

	// Disable compression
	options.compression = rocksdb::kNoCompression;

	// Write buffers adaptés aux gros objets
	options.write_buffer_size = 512ULL * 1024 * 1024;
	options.max_write_buffer_number = 3;

	// --- BLOB CONFIG ---
	options.enable_blob_files = true;
	options.min_blob_size = 4096; // 4KB
	options.blob_file_size = 512ULL * 1024 * 1024;
	options.blob_compression_type = rocksdb::kNoCompression;

	options.enable_blob_garbage_collection = false;

	// Table options
	rocksdb::BlockBasedTableOptions table_options;
	table_options.block_size = 64 * 1024;
	table_options.cache_index_and_filter_blocks = false;

	options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

	// Cleanup DB
	rocksdb::DestroyDB(db_path, options);
	std::cout << "[INFO] DB cleaned up\n";

	// === Open DB ===
	rocksdb::DB* db;
	rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);

	if (!status.ok()) {
		std::cerr << "[ERROR] Error opening DB: " << status.ToString() << "\n";
		return EXIT_FAILURE;
	}

	std::cout << "[INFO] RocksDB is ready!\n";
	std::cout << "[INFO] --------------------------------------\n"
			  << "[INFO] Number of blocks : " << num_blobs << "\n"
			  << "[INFO] Block size       : " << blob_size << "\n"
			  << "[INFO] Input file       : " << input_file << "\n"
			  << "[INFO] Database path    : " << db_path << "\n"
			  << "[INFO] --------------------------------------\n";

	rocksdb::WriteOptions write_opts;
	write_opts.disableWAL = true;

	// Blobs generation
	for (uint64_t i = 0; i < num_blobs; ++i) {
		std::string key = format_key(i);

		status = db->Put(write_opts, rocksdb::Slice(key), rocksdb::Slice(buffer.data(), blob_size));

		if (!status.ok()) {
			std::cerr << "[ERROR] Write error " << key << " : " << status.ToString() << "\n";
			delete db;
			return EXIT_FAILURE;
		}

		if (i % (num_blobs / 10) == 0) {
			std::cout << "[INFO] Progression : " << std::ceil((i * 100.0) / num_blobs) << "%\n";
		}
	}
	std::cout << "[INFO] Data written : " << (num_blobs * blob_size) / (1024 * 1024)
			  << "MB total\n";

	delete db;
	return EXIT_SUCCESS;
}