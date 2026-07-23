#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <string>
#include <optional>

#include <filters.h>
#include <image.h>
#include <threadpool.h>

enum FilterType { GrayScale, GaussianBlur, Sobel };

struct Args {
  std::string input_path;
  std::string output_path;
  FilterType filter = FilterType::GrayScale;
  std::optional<int> kernel_size;
  std::optional<BorderMode> border_mode = BorderMode::BORDER_REFLECT;
  std::optional<float> sigmaX;
  std::optional<float> sigmaY = 0.0f;
  std::optional<int> dx;
  std::optional<int> dy;
  std::optional<double> scale = 1.0;
  std::optional<double> delta = 0.0;

  int num_threads = 0;

  bool benchmark = false;
  std::string csv;
};

void printUsage(const char* program) {
  std::cout << "\nUsage:\n"
            << "  " << program
            << " --input <file> --output <file> --filter <filter> [options]\n\n"

            << "Required arguments:\n"
            << "  --input <file>          Input image path\n"
            << "  --output <file>         Output image path\n"
            << "  --filter <filter>       Filter to apply\n"
            << "                          grayscale | blur | sobel\n\n"

            << "Filter-specific options:\n"
            << "  Blur:\n"
            << "    --kernel_size <int>   Kernel size (odd positive integer)\n"
            << "    --sigmaX <float>      Sigma in X direction\n"
            << "    --sigmaY <float>      Sigma in Y direction\n"
            << "    --border <mode>       Border handling mode\n\n"

            << "  Sobel:\n"
            << "    --dx <int>            Derivative order in X\n"
            << "    --dy <int>            Derivative order in Y\n"
            << "    --kernel_size <int>   Kernel size\n"
            << "    --scale <double>      Scale factor\n"
            << "    --delta <double>      Delta value\n"
            << "    --border <mode>       Border handling mode\n\n"

            << "Border modes:\n"
            << "  clamp | reflect | mirror | wrap | constant\n\n"

            << "Other options:\n"
            << "  --threads <n>           Number of worker threads\n"
            << "  --benchmark             Enable benchmarking\n"
            << "  --csv <file>            Save benchmark results to CSV\n"
            << "  -h, --help              Show this help message\n\n"

            << "Examples:\n"
            << "  " << program
            << " --input in.png --output out.png --filter grayscale\n\n"

            << "  " << program
            << " --input in.png --output out.png --filter blur"
            << " --kernel_size 5 --sigmaX 1.5 --sigmaY 1.5\n\n"

            << "  " << program
            << " --input in.png --output out.png --filter sobel"
            << " --dx 1 --dy 0 --kernel_size 3\n";
}

// ============== HELPER FUNCTIONS FOR PRINTING =================
static const char* toString(FilterType filter) {
  switch (filter) {
  case FilterType::GrayScale:
    return "grayscale";
  case FilterType::GaussianBlur:
    return "blur";
  case FilterType::Sobel:
    return "sobel";
  default:
    return "unknown";
  }
}

static const char* toString(BorderMode mode) {
  switch (mode) {
  case BorderMode::BORDER_CLAMP:
    return "clamp";
  case BorderMode::BORDER_REFLECT:
    return "reflect";
  case BorderMode::BORDER_MIRROR:
    return "mirror";
  case BorderMode::BORDER_WRAP:
    return "wrap";
  case BorderMode::BORDER_CONSTANT:
    return "constant";
  default:
    return "unknown";
  }
}

void printArgs(const Args& args) {
  auto printOptional = [](const auto& opt) {
    if (opt)
      std::cout << *opt;
    else
      std::cout << "<not set>";
  };

  std::cout << "Arguments:\n";
  std::cout << "----------------------------------------\n";
  std::cout << "Input Path   : " << args.input_path << '\n';
  std::cout << "Output Path  : " << args.output_path << '\n';
  std::cout << "Filter       : " << toString(args.filter) << '\n';

  std::cout << "Kernel Size  : ";
  printOptional(args.kernel_size);
  std::cout << '\n';

  std::cout << "Border Mode  : ";
  if (args.border_mode)
    std::cout << toString(*args.border_mode);
  else
    std::cout << "<not set>";
  std::cout << '\n';

  std::cout << "Sigma X      : ";
  printOptional(args.sigmaX);
  std::cout << '\n';

  std::cout << "Sigma Y      : ";
  printOptional(args.sigmaY);
  std::cout << '\n';

  std::cout << "dx           : ";
  printOptional(args.dx);
  std::cout << '\n';

  std::cout << "dy           : ";
  printOptional(args.dy);
  std::cout << '\n';

  std::cout << "Scale        : ";
  printOptional(args.scale);
  std::cout << '\n';

  std::cout << "Delta        : ";
  printOptional(args.delta);
  std::cout << '\n';

  std::cout << "Threads      : " << args.num_threads << '\n';
  std::cout << "Benchmark    : " << (args.benchmark ? "true" : "false") << '\n';
  std::cout << "CSV File     : " << (args.csv.empty() ? "<not set>" : args.csv)
            << '\n';
  std::cout << "----------------------------------------\n";
}

// ==============================================================

std::optional<Args> parseArgs(int argc, char** argv) {
  Args args;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "--input") {
      args.input_path = argv[++i];
    } else if (arg == "--output") {
      args.output_path = argv[++i];
    } else if (arg == "--filter") {
      std::string filter = argv[++i];

      if (filter == "grayscale") {
        args.filter = FilterType::GrayScale;
      } else if (filter == "blur") {
        args.filter = FilterType::GaussianBlur;
      } else if (filter == "sobel") {
        args.filter = FilterType::Sobel;
      } else {
        throw std::runtime_error("Unknown filter: " + filter);
      }
    } else if (arg == "--kernel_size") {
      args.kernel_size = std::stoi(argv[++i]);
    } else if (arg == "--border") {
      std::string border = argv[++i];

      if (border == "clamp") {
        args.border_mode = BorderMode::BORDER_CLAMP;
      } else if (border == "reflect") {
        args.border_mode = BorderMode::BORDER_REFLECT;
      } else if (border == "mirror") {
        args.border_mode = BorderMode::BORDER_MIRROR;
      } else if (border == "wrap") {
        args.border_mode = BorderMode::BORDER_WRAP;
      } else if (border == "constant") {
        args.border_mode = BorderMode::BORDER_CONSTANT;
      } else {
        throw std::runtime_error("Unknown border mode: " + border);
      }

    } else if (arg == "--sigmaX") {
      args.sigmaX = std::stof(argv[++i]);
    } else if (arg == "--sigmaY") {
      args.sigmaY = std::stof(argv[++i]);
    } else if (arg == "--dx") {
      args.dx = std::stoi(argv[++i]);
    } else if (arg == "--dy") {
      args.dy = std::stoi(argv[++i]);
    } else if (arg == "--scale") {
      args.scale = std::stod(argv[++i]);
    } else if (arg == "--delta") {
      args.delta = std::stod(argv[++i]);
    } else if (arg == "--threads") {
      args.num_threads = std::stoi(argv[++i]);
    } else if (arg == "--benchmark") {
      args.benchmark = true;
    } else if (arg == "--csv") {
      args.csv = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      return std::nullopt;
    } else {
      throw std::runtime_error("Invalid argument: " + arg);
    }
  }

  // Verify the arguments
  if (args.input_path.empty() || args.output_path.empty()) {
    throw std::runtime_error("--input and --output are required");
  }

  if (args.filter == FilterType::GaussianBlur) {
    if (!args.sigmaX || !args.kernel_size) {
      throw std::runtime_error(
          "--kernel-size and --sigmaX are required "
          "for blur filter");
    }
  }

  if (args.filter == FilterType::Sobel) {
    if (!args.dx || !args.dy || !args.kernel_size) {
      throw std::runtime_error(
          "--kernel-size, --dx and --dy are required "
          "for sobel filter");
    }
  }

  return args;
}

int main(int argc, char** argv) {
  // parse params
  Args args;
  try {
    auto parsed = parseArgs(argc, argv);

    if (!parsed) {
      printUsage(argv[0]);
      return 0;
    }

    args = *parsed;
  } catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    printUsage(argv[0]);
    return 1;
  }

  printArgs(args);

  unsigned int num_threads = args.num_threads;
  if (num_threads == 0) {
    num_threads = std::max(1u, std::thread::hardware_concurrency());
  } else {
    num_threads = std::max(num_threads, std::thread::hardware_concurrency());
  }

  ThreadPool pool(num_threads);

  Image in_img;
  in_img.load(args.input_path);

  int out_channels = in_img.getChannels();
  if (args.filter == FilterType::GrayScale) {
    out_channels = 1;
  }

  Image out_img(in_img.getWidth(), in_img.getHeight(), out_channels);

  auto runFilter = [&]() {
    switch (args.filter) {
    case FilterType::GrayScale:
      grayscale(out_img, in_img, pool, num_threads);
      break;
    case FilterType::GaussianBlur:
      gaussian_blur(out_img, in_img, pool, num_threads,
                    args.kernel_size.value(), args.sigmaX.value(),
                    args.sigmaY.value(), args.border_mode.value());
      break;
    case FilterType::Sobel: {
      // Allocate temp buffer for X axis
      int width = in_img.getWidth();
      int height = in_img.getHeight();
      int channels = in_img.getChannels();
      size_t total = static_cast<size_t>(width) * height * channels;

      std::vector<double> gx(total), gy(total);

      sobel<double>(gx, in_img, pool, num_threads, /*dx=*/1, /*dy=*/0,
                    args.kernel_size.value(), args.scale.value(),
                    args.delta.value(),
                    args.border_mode.value_or(BorderMode::BORDER_REFLECT));
      sobel<double>(gy, in_img, pool, num_threads, /*dx=*/0, /*dy=*/1,
                    args.kernel_size.value(), args.scale.value(),
                    args.delta.value(),
                    args.border_mode.value_or(BorderMode::BORDER_REFLECT));

      auto* out_data = out_img.getDataMutable();
      for (size_t i = 0; i < total; i++) {
        double mag = std::sqrt(gx[i] * gx[i] + gy[i] * gy[i]);
        out_data[i] = static_cast<uint8_t>(std::clamp(mag, 0.0, 255.0));
      }
    } break;
    default:
      throw std::runtime_error("Invalid filter!");
    }
  };

  // runfilter or benchmark as per the given mode
  if (!args.benchmark) {
    std::cout << "Running filter: " << toString(args.filter) << std::endl;
    runFilter();
    std::cout << "Saving image...";
    out_img.save(args.output_path);
    std::cout << "Done!" << std::endl;
    return 0;
  }
}