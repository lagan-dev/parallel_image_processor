// // engine.cpp
// #include <algorithm>  // std::sort (used in benchmark median calc)
// #include <fstream>  // std::ifstream, std::ofstream (used in benchmark CSV writing)
// #include <chrono>
// #include <condition_variable>
// #include <cstring>
// #include <functional>
// #include <iostream>
// #include <mutex>
// #include <optional>
// #include <queue>
// #include <stdexcept>
// #include <string>
// #include <thread>
// #include <vector>

// #include "image.h"    // Image, BorderMode
// #include "filters.h"  // gaussian_blur, sobel declarations
// #include "threadpool.h"  // threadpool

// // ---------------------------------------------------------------------------
// // parallel_for_rows
// // ---------------------------------------------------------------------------
// template <typename Func>
// void parallel_for_rows(ThreadPool& pool, int total_rows, Func&& f,
//                        unsigned int num_threads = 0) {
//   if (num_threads == 0) num_threads = static_cast<unsigned int>(pool.size());
//   num_threads = std::min<unsigned int>(num_threads, std::max(total_rows, 1));

//   if (num_threads <= 1 || total_rows == 0) {
//     f(0, total_rows);
//     return;
//   }

//   int chunk = (total_rows + static_cast<int>(num_threads) - 1) /
//               static_cast<int>(num_threads);
//   for (unsigned int t = 0; t < num_threads; t++) {
//     int start = static_cast<int>(t) * chunk;
//     int end = std::min(start + chunk, total_rows);
//     if (start >= end) break;
//     pool.enqueue([start, end, &f]() { f(start, end); });
//   }
//   pool.waitAll();
// }

// // ---------------------------------------------------------------------------
// // CLI args
// // ---------------------------------------------------------------------------
// enum class FilterType { GaussianBlur, Sobel };

// struct Args {
//   std::string input_path;
//   std::string output_path;
//   FilterType filter = FilterType::GaussianBlur;
//   int kernel_size = 5;
//   float sigmaX = 1.0f;
//   float sigmaY = 0.0f;
//   BorderMode border_mode = BorderMode::BORDER_REFLECT;
//   unsigned int num_threads = 0;  // 0 => hardware_concurrency
//   bool benchmark = false;
//   std::string csv_path;
// };

// void printUsage(const char* prog) {
//   std::cerr << "Usage: " << prog
//             << " --input <path> --output <path> --filter <blur|sobel>\n"
//                "           [--kernel-size N] [--sigma X] [--border "
//                "reflect|constant|replicate|wrap]\n"
//                "           [--threads N] [--benchmark --csv <path>]\n";
// }

// std::optional<Args> parseArgs(int argc, char** argv) {
//   Args args;
//   for (int i = 1; i < argc; i++) {
//     std::string arg = argv[i];
//     auto next = [&]() -> std::string {
//       if (i + 1 >= argc) throw std::runtime_error("missing value for " + arg);
//       return argv[++i];
//     };

//     if (arg == "--input") {
//       args.input_path = next();
//     } else if (arg == "--output") {
//       args.output_path = next();
//     } else if (arg == "--filter") {
//       std::string v = next();
//       if (v == "blur")
//         args.filter = FilterType::GaussianBlur;
//       else if (v == "sobel")
//         args.filter = FilterType::Sobel;
//       else
//         throw std::runtime_error("unknown filter: " + v);
//     } else if (arg == "--kernel-size") {
//       args.kernel_size = std::stoi(next());
//     } else if (arg == "--sigma") {
//       args.sigmaX = std::stof(next());
//     } else if (arg == "--border") {
//       std::string v = next();
//       if (v == "reflect")
//         args.border_mode = BorderMode::BORDER_REFLECT;
//       else if (v == "mirror")
//         args.border_mode = BorderMode::BORDER_MIRROR;
//       else if (v == "constant")
//         args.border_mode = BorderMode::BORDER_CONSTANT;
//       else if (v == "clamp" || v == "replicate")
//         args.border_mode = BorderMode::BORDER_CLAMP;
//       else if (v == "wrap")
//         args.border_mode = BorderMode::BORDER_WRAP;
//       else
//         throw std::runtime_error("unknown border mode: " + v);
//     } else if (arg == "--threads") {
//       args.num_threads = static_cast<unsigned int>(std::stoul(next()));
//     } else if (arg == "--benchmark") {
//       args.benchmark = true;
//     } else if (arg == "--csv") {
//       args.csv_path = next();
//     } else if (arg == "--help" || arg == "-h") {
//       return std::nullopt;
//     } else {
//       throw std::runtime_error("unrecognized argument: " + arg);
//     }
//   }

//   if (args.input_path.empty() || args.output_path.empty()) {
//     throw std::runtime_error("--input and --output are required");
//   }
//   return args;
// }

// // ---------------------------------------------------------------------------
// // main
// // ---------------------------------------------------------------------------
// int main(int argc, char** argv) {
//   Args args;
//   try {
//     auto parsed = parseArgs(argc, argv);
//     if (!parsed) {
//       printUsage(argv[0]);
//       return 0;
//     }
//     args = *parsed;
//   } catch (const std::exception& e) {
//     std::cerr << "Error: " << e.what() << "\n";
//     printUsage(argv[0]);
//     return 1;
//   }

//   unsigned int num_threads = args.num_threads;
//   if (num_threads == 0) {
//     num_threads = std::max(1u, std::thread::hardware_concurrency());
//   }

//   Image input;
//   input.load(args.input_path);
//   Image output(input.getWidth(), input.getHeight(), input.getChannels());

//   ThreadPool pool(num_threads);

//   double borderValue[4] = {0, 0, 0, 0};

//   auto runFilter = [&]() {
//     switch (args.filter) {
//     case FilterType::GaussianBlur:
//       // gaussian_blur(output, input, args.kernel_size, args.sigmaX, args.sigmaY,
//       //               args.border_mode, borderValue, num_threads, pool);
//       break;
//     case FilterType::Sobel:
//       // sobel(output, input, args.border_mode, borderValue, num_threads, pool);
//       break;
//     }
//   };

//   if (!args.benchmark) {
//     runFilter();
//     output.save(args.output_path);  // adjust to your actual save API
//     return 0;
//   }

//   // --- benchmark mode: median over several runs, write CSV ---
//   constexpr int kRuns = 7;
//   std::vector<double> samples_ms;
//   samples_ms.reserve(kRuns);

//   for (int i = 0; i < kRuns; i++) {
//     auto t0 = std::chrono::steady_clock::now();
//     runFilter();
//     auto t1 = std::chrono::steady_clock::now();
//     samples_ms.push_back(
//         std::chrono::duration<double, std::milli>(t1 - t0).count());
//   }

//   std::sort(samples_ms.begin(), samples_ms.end());
//   double median_ms = samples_ms[samples_ms.size() / 2];

//   std::cout << "median: " << median_ms << " ms over " << kRuns
//             << " runs, threads=" << num_threads << "\n";

//   if (!args.csv_path.empty()) {
//     bool file_exists = std::ifstream(args.csv_path).good();
//     std::ofstream csv(args.csv_path, std::ios::app);
//     if (!file_exists) {
//       csv << "filter,threads,kernel_size,median_ms\n";
//     }
//     csv << (args.filter == FilterType::GaussianBlur ? "gaussian_blur" : "sobel")
//         << "," << num_threads << "," << args.kernel_size << "," << median_ms
//         << "\n";
//   }

//   output.save(args.output_path);
//   return 0;
// }