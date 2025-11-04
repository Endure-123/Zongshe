#include "BookShelfExporter.h"
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <unordered_set>

using namespace std;

namespace bookshelf {

    static void EnsureDir(const std::filesystem::path& dir, bool create) {
        if (std::filesystem::exists(dir)) {
            if (!std::filesystem::is_directory(dir))
                throw runtime_error("Output path exists but is not a directory: " + dir.string());
        }
        else if (create) {
            std::filesystem::create_directories(dir);
        }
        else {
            throw runtime_error("Output directory does not exist: " + dir.string());
        }
    }

    static void Validate(const vector<Node>& nodes, const vector<Net>& nets) {
        unordered_set<string> nodeNames;
        for (const auto& n : nodes) {
            if (n.name.empty()) throw runtime_error("Node name is empty");
            if (!nodeNames.insert(n.name).second)
                throw runtime_error("Duplicated node name: " + n.name);
            if (!n.terminal && (n.width <= 0 || n.height <= 0))
                throw runtime_error("Non-terminal node has non-positive size: " + n.name);
        }
        for (const auto& net : nets) {
            if (net.pins.empty()) throw runtime_error("Net has zero pins: " + net.name);
            for (const auto& p : net.pins) {
                if (p.cellName.empty())
                    throw runtime_error("Pin with empty cellName in net: " + net.name);
            }
        }
    }

    static void WriteNodes(const filesystem::path& file,
        const vector<Node>& nodes, int precision) {
        size_t numNodes = nodes.size();
        size_t numTerms = 0;
        for (auto& n : nodes) if (n.terminal) ++numTerms;

        ofstream ofs(file);
        if (!ofs) throw runtime_error("Cannot open file: " + file.string());
        ofs << "UCLA nodes 1.0\n";
        ofs << "NumNodes : " << numNodes << "\n";
        ofs << "NumTerminals : " << numTerms << "\n\n";

        ofs << fixed << setprecision(precision);
        for (const auto& n : nodes) {
            ofs << n.name << " " << n.width << " " << n.height;
            if (n.terminal) ofs << " terminal";
            ofs << "\n";
        }
    }

    static void WriteNets(const filesystem::path& file,
        const vector<Net>& nets, int precision) {
        size_t totalPins = 0;
        for (const auto& net : nets) totalPins += net.pins.size();

        ofstream ofs(file);
        if (!ofs) throw runtime_error("Cannot open file: " + file.string());
        ofs << "UCLA nets 1.0\n";
        ofs << "NumNets : " << nets.size() << "\n";
        ofs << "NumPins : " << totalPins << "\n\n";

        ofs << fixed << setprecision(precision);
        for (size_t i = 0; i < nets.size(); ++i) {
            const auto& net = nets[i];
            const string netName = !net.name.empty() ? net.name : ("n" + to_string(i));
            ofs << "NetDegree : " << net.pins.size() << " " << netName << "\n";
            for (const auto& p : net.pins) {
                const string pinName = p.pinName.empty() ? "P" : p.pinName;
                ofs << " " << p.cellName << " " << pinName << " : "
                    << p.dx << " " << p.dy << "\n";
            }
            ofs << "\n";
        }
    }

    std::pair<std::filesystem::path, std::filesystem::path>
        ExportBookShelf(const std::string& projectName,
            const std::vector<Node>& nodes,
            const std::vector<Net>& nets,
            const ExportOptions& opt)
    {
        Validate(nodes, nets);
        EnsureDir(opt.outDir, opt.forceCreateDir);

        const auto nodesPath = std::filesystem::absolute(opt.outDir / (projectName + ".nodes"));
        const auto netsPath = std::filesystem::absolute(opt.outDir / (projectName + ".nets"));

        WriteNodes(nodesPath, nodes, opt.floatPrecision);
        WriteNets(netsPath, nets, opt.floatPrecision);
        return { nodesPath, netsPath };
    }

} // namespace bookshelf
