#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>
// #include <random>
#include <chrono>
#include <set>

using namespace std;

namespace Limits
{
    using IDType = uint16_t;
    auto constexpr LowerBound = std::numeric_limits<IDType>::min();
    auto constexpr UpperBound = std::numeric_limits<IDType>::max();
}

auto parseCommandLine(int argc, char** argv)
{
    if (argc != 2)
    {
        throw std::runtime_error(string{"Usage: "} + string{argv[0]} + string{" <input-file>"});
    }
    auto inputPath = string{argv[1]};
    return filesystem::path{inputPath};
}

auto parseInput(filesystem::path const& path)
{
    auto handleInputError = [&path] (string const& message)
    {
        throw std::runtime_error("Error '" + message + "' for path '" + path.string() + "'");
    };

    if (filesystem::exists(path) == false)
    {
        handleInputError("Does not exist");
    }
    if (filesystem::is_directory(path))
    {
        handleInputError("Is a directory");
    }
    if (filesystem::is_regular_file(path) == false)
    {
        handleInputError("Not a regular file");
    }

    std::ifstream stream{path, ifstream::in};
    if (stream.good() == false)
    {
        handleInputError("Failed to open");
    }
    string line;
    if (!getline(stream, line))
    {
        handleInputError("Failed to read first line");
    }
    int count = 0;
    try
    {
        count = stoi(line);
    }
    catch (std::exception const&)
    {
        handleInputError("Failed to parse count at line 0: " + line);
    }
    if (count < Limits::LowerBound || count > Limits::UpperBound)
    {
        handleInputError("Count does not fit in an unsigned 16-bit integer: " + std::to_string(count));
    }
    std::cout << "Count: " << count << std::endl;
    vector<float> delays;
    for (int i=0; i<count; ++i)
    {
        if (!getline(stream, line))
        {
            handleInputError("Failed to read the delay of node with index " + std::to_string(i) + " expected at line " + std::to_string(i+2));
        }
        float delay = 0;
        try
        {
            delay = std::stof(line);
        }
        catch (std::exception const&)
        {
            handleInputError("Failed to parse a floating-point number at line " + std::to_string(i+2));
        }
        delays.emplace_back(delay);
        // std::cout << "Delay " << i << " delay " << delay << std::endl;
    }
    return delays;
}

class Node
{
private:
    static auto generateID()
    {
        /* random number : no guarantee on uniqueness
        static std::mt19937 random_generator = [] () {
            auto ret = std::mt19937 { std::random_device{}() };
            ret.seed(time(0));
            return ret;
        }();

        std::uniform_int_distribution<Limits::IDType> distrib { Limits::LowerBound, Limits::UpperBound };
        return distrib(random_generator);
        */

        static std::chrono::_V2::high_resolution_clock clock;
        auto ticks = clock.now().time_since_epoch().count();
        ticks %= Limits::UpperBound;
        // std::cout << "clock " << ticks << std::endl;
        return ticks;
    }
    static auto generatePort()
    {
        static int start = 1025;
        return start++;
    }
public:
    Node(float delay) :
        id(generateID()),
        port(generatePort()),
        delay(delay)
    {
        // cout << "Node " << id << " port " << port <<  " delay " << delay << endl;
    }
    void link(int neighborPort)
    {
        this->neighborPort = neighborPort;
    }
    void print() const
    {
        cout << "Node " << id << " port " << port <<  " listening to " << neighborPort <<  " delay " << delay << endl;
    }
    auto getPort() const { return port; }
    auto getID() const { return id; }
private:
    Limits::IDType const id;
    int const port;
    float const delay;
    int neighborPort = 0;
};

auto generateNodes(vector<float> const& delays)
{
    vector<Node> nodes;
    for (auto const& delay: delays)
    {
        nodes.emplace_back(delay);
    }
    return nodes;
}

void verifyUniqueIDs(vector<Node> const& nodes)
{
    set<Limits::IDType> ids;
    for (auto const& node: nodes)
    {
        auto id = node.getID();
        if (ids.count(id))
        {
            throw std::runtime_error("This ID is not unique, assigned to multiple nodes: " + std::to_string(id));
        }
        ids.insert(id);
    }
}

void linkNodes(vector<Node>& nodes)
{
    for (size_t i=0; i<nodes.size(); ++i)
    {
        // std::cout << "index " << i << " neighbor " << (i + nodes.size() - 1) % nodes.size() << std::endl;
        auto const& counterClockwiseNeighbor =
            nodes.at((i + nodes.size() - 1) % nodes.size());
        nodes[i].link(counterClockwiseNeighbor.getPort());
    }
    for (auto const& node: nodes)
    {
        node.print();
    }
}

int main(int argc, char** argv)
{
    auto inputFile = parseCommandLine(argc, argv);

    auto delays = parseInput(inputFile);

    auto nodes = generateNodes(delays);

    verifyUniqueIDs(nodes);

    linkNodes(nodes);

    return 0;
}
