#include "BookShelfImporter.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>

using namespace std;

namespace bookshelf {

    static inline string trim(const string& s) {
        size_t b = s.find_first_not_of(" \t\r\n");
        if (b == string::npos) return "";
        size_t e = s.find_last_not_of(" \t\r\n");
        return s.substr(b, e - b + 1);
    }

    static void ParseNodes(const filesystem::path& p, BSDesign& d) {
        ifstream ifs(p);
        if (!ifs) throw runtime_error("Cannot open .nodes: " + p.string());
        string line;
        while (getline(ifs, line)) {
            line = trim(line);
            if (line.empty()) continue;
            if (line[0] == '#') continue;
            if (line.rfind("UCLA", 0) == 0) continue;
            if (line.rfind("NumNodes", 0) == 0) continue;
            if (line.rfind("NumTerminals", 0) == 0) continue;

            // 形如： name width height [terminal]
            istringstream ss(line);
            BSNode n;
            ss >> n.name >> n.width >> n.height;
            string word;
            if (ss >> word) {
                if (word == "terminal" || word == "terminal:") n.terminal = true;
            }
            if (!n.name.empty()) {
                d.nodeIndexByName[n.name] = d.nodes.size();
                d.nodes.push_back(n);
            }
        }
    }

    static void ParseNets(const filesystem::path& p, BSDesign& d) {
        ifstream ifs(p);
        if (!ifs) throw runtime_error("Cannot open .nets: " + p.string());
        string line;
        BSNet current;
        int expectPins = -1;

        auto flush = [&]() {
            if (!current.name.empty() && !current.pins.empty()) {
                d.nets.push_back(current);
            }
            current = BSNet{};
            expectPins = -1;
            };

        while (getline(ifs, line)) {
            line = trim(line);
            if (line.empty()) continue;
            if (line[0] == '#') continue;
            if (line.rfind("UCLA", 0) == 0) continue;
            if (line.rfind("NumNets", 0) == 0) continue;
            if (line.rfind("NumPins", 0) == 0) continue;

            if (line.rfind("NetDegree", 0) == 0) {
                // NetDegree : <k> <name>
                flush();
                istringstream ss(line);
                string token, colon;
                ss >> token >> colon; // NetDegree :
                ss >> expectPins >> current.name;
                if (current.name.empty()) current.name = "net";
                continue;
            }

            // 引脚行： <cell> <pin> : dx dy
            // pin 可能紧挨着冒号或中间有空格，兼容处理
            // 例：O441 I : -0.5 -6.0  或  o441 I:-0.5 -6.0
            {
                BSPin pin;
                // 先拆出 cell 与剩余
                istringstream ss(line);
                ss >> pin.cellName;
                string rest; getline(ss, rest);
                rest = trim(rest);

                // 找到 ':' 位置
                auto posColon = rest.find(':');
                if (posColon == string::npos) continue;
                string left = trim(rest.substr(0, posColon));  // 可能是 "I" 或 "I " 或 "I"
                string right = trim(rest.substr(posColon + 1)); // dx dy

                // pinName 可能写成 I 或 I
                // 也可能是 I:-0.5 这种被我们切开了
                {
                    // left 末尾若有空格，去掉
                    if (!left.empty() && left.back() == ' ') left.pop_back();
                    pin.pinName = left;
                }
                {
                    istringstream rs(right);
                    rs >> pin.dx >> pin.dy;
                }
                current.pins.push_back(pin);

                if ((int)current.pins.size() == expectPins) {
                    flush();
                }
            }
        }
        flush();
    }

    BSDesign ParseBookShelf(const filesystem::path& nodesPath,
        const filesystem::path& netsPath)
    {
        BSDesign d;
        ParseNodes(nodesPath, d);
        ParseNets(netsPath, d);
        return d;
    }

} // namespace bookshelf
