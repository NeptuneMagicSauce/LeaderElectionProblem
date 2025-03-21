#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>
// #include <random>
#include <chrono>
#include <set>
#include<thread>
#include <mutex>
#include <queue>
// #include <sys/socket.h> ouch no!
// QT sockets: sudo apt install qt6-base-dev
#include <QTcpSocket>
#include <QTcpServer>

#include "json.hpp"

using namespace std;
using namespace json;

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
        handleInputError("Failed to parse count at line 1: " + line);
    }
    if (count < Limits::LowerBound || count > Limits::UpperBound)
    {
        handleInputError("Count does not fit in an unsigned 16-bit integer: " + std::to_string(count));
    }
    std::cout << "Count: " << count << std::endl;
    if (count == 0)
    {
        handleInputError("No election with zero voter");
    }
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
    using ID = Limits::IDType;

    static auto generateID()
    {
        /* random number : no guarantee on uniqueness
        static std::mt19937 random_generator = [] () {
            auto ret = std::mt19937 { std::random_device{}() };
            ret.seed(time(0));
            return ret;
        }();

        std::uniform_int_distribution<ID> distrib { Limits::LowerBound, Limits::UpperBound };
        return distrib(random_generator);
        */

        static std::chrono::_V2::high_resolution_clock clock;
        auto ticks = clock.now().time_since_epoch().count();
        ticks *= ticks; // so that it's not so neatly ordered
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
    }
    void linkAndStart(int neighborPort)
    {
        this->neighborPort = neighborPort;
        processThread = std::make_shared<thread>(&Node::process, this);
    }
    void printDescription() const
    {
        ostringstream s;
        s << "Starting to talk to port " << port <<  " and to listen to " << neighborPort <<  " with delay " << delay;
        PrintSafely(s.str());
    }
    auto getPort() const { return port; }
    auto getID() const { return id; }
    auto getFinished() const { return finished; }
    auto getLeader() const { return leader; }
    void join()
    {
        processThread->join();
        talkThread->join();
        listenThread->join();
    }

    void PrintSafely(string const& message) const
    {
        lock_guard<mutex> guard{printLock};
        ostringstream s;
        s <<
            "[" <<
            std::setfill('0') << std::setw(5) <<
            to_string(id) <<
            "] " << message << std::endl;
        cout << s.str();
        try
        {
            // in production code, I would have a lock for each of the sinks: cout and the file
            // for fewer lock contention
            ofstream log{outputPath, ios_base::app};
            log << s.str();
        }
        catch (std::exception const&) { }
    }

    static string const outputPath;
private:
    ID const id;
    int const port;
    float const delay;
    int neighborPort = 0;
    shared_ptr<thread> processThread;
    shared_ptr<thread> talkThread;
    shared_ptr<thread> listenThread;
    struct MessageQueue
    {
        queue<Message> messages;
        shared_ptr<mutex> lock = make_shared<mutex>();
        void push(Message const& msg)
        {
            lock_guard<mutex> guard{*lock};
            messages.push(msg);
        }
        optional<Message> pop()
        {
            lock_guard<mutex> guard{*lock};
            if (messages.empty())
            {
                return {};
            }
            auto msg = messages.front();
            messages.pop();
            return msg;
        }
        auto size()
        {
            lock_guard<mutex> guard{*lock};
            return messages.size();
        }
    };
    MessageQueue sendQueue;
    MessageQueue receiveQueue;
    shared_ptr<QTcpSocket> talkSocket;
    shared_ptr<QTcpServer> listenServer;
    QHostAddress const localhost = QHostAddress::LocalHost;
    set<ID> peers;
    optional<ID> leader;
    bool finished = false;
    enum struct Direction { Receive, Send };
    int const dataStreamVersion = QDataStream::Qt_5_10;
    static mutex printLock;

    void sleep() { std::this_thread::sleep_for(200ms); }

    void printMessage(Message const& msg, Direction direction, string const& action)
    {
        if (msg.type == Message::Type::Greetings)
        {
            return;
        }

        ostringstream s;
        if (direction == Direction::Receive)
        {
            s << "received ";
        }
        else if (direction == Direction::Send)
        {
            s << "sending ";
        }
        if (msg.type == Message::Type::Greetings)
        {
            s << "Greetings";
        }
        else if (msg.type == Message::Type::ElectionStart)
        {
            s << "ElectionStart";
        }
        else if (msg.type == Message::Type::ElectedLeader)
        {
            s << "ElectedLeader";
        }
        s << " from " <<
            std::setfill('0') << std::setw(5) <<
            to_string(msg.id);
        if (action.length())
        {
            s << ", " << action;
        }
        PrintSafely(s.str());
    }

    void listen()
    {
        listenServer = make_shared<QTcpServer>();
        if (listenServer->listen(localhost, neighborPort) == false)
        {
            throw std::runtime_error(to_string(id) + " listen thread Failed to listen on port " + to_string(neighborPort));
        }
        PrintSafely("listening on port " + to_string(neighborPort));

        listenServer->waitForNewConnection(3000);
        auto socket = listenServer->nextPendingConnection();
        if (socket == nullptr)
        {
            throw std::runtime_error(to_string(id) +  " listenThread nobody connected before timeout");
        }

        while (finished == false)
        {
            sleep();
            ostringstream s;
            s << "\tlisten\t\t";
            // s << "state " << (int)socket->state();
            QDataStream in(socket);
            in.setVersion(dataStreamVersion);
            QString msg;
// #warning "do we need a loop and commitTransaction() ?"
            do
            {
                if (socket->waitForReadyRead(1))
                {
                    in.startTransaction();
                    in >> msg;
                    s << " READ '";
                    s << msg.toStdString() << "'";
                    // PrintSafely(s.str());
                }
                else
                {
                    break;
                }
            } while (in.commitTransaction() == false);
            // }

            if (msg.length())
            {
                // PrintSafely(s.str());
                auto asMessage = from_string(msg.toStdString());
                if (asMessage)
                {
                    receiveQueue.push(*asMessage);
                }
                else
                {
                    PrintSafely("Failed to parse json: " + msg.toStdString());
                }
            }
        }
        PrintSafely("end listen thread");
    }

    void talk()
    {
        auto& socket = talkSocket;
        socket = make_shared<QTcpSocket>();
        // PrintSafely("localhost = " + localhost.toStdString());
        socket->connectToHost(localhost, port);
        PrintSafely("talking on port " + to_string(port));
        if (socket->waitForConnected(3000) == false)
        {
            throw std::runtime_error(to_string(id) + " talk thread Failed to connect socket to port " + to_string(port));
        }
        while (finished == false)
        {
            sleep();
            ostringstream s;
            s << "\ttalk\t\t";
            // s << " socket state " << (int)socket->state();

            auto message = sendQueue.pop();
            if (message)
            {
                printMessage(*message, Direction::Send, "still in queue: " + to_string(sendQueue.size()));
// #warning "re enable delay"
                std::this_thread::sleep_for(chrono::milliseconds(static_cast<int>(1000 * delay)));
                QByteArray block;
                QDataStream out(&block, QIODevice::WriteOnly);
                out.setVersion(dataStreamVersion);
                auto asString = to_string(*message);
                // auto asString = string{"message defg"};
                out << QString::fromStdString(asString);
                socket->write(block);
                s << " WRITTEN '" << asString << "'";

                socket->waitForBytesWritten();
                // socket->flush();

                // PrintSafely(s.str());
            }
        }
        socket->disconnectFromHost();
        PrintSafely("end talk thread");
    }

    void process()
    {
        enum struct State
        {
            Offline,
            Participating,
            Decided,
            Leader,
        } state = State::Offline;

        peers.insert(id);

        // auto startTime =  std::chrono::system_clock::now();
        auto allReady = false;

        // spec says "then it will send a message indicating its unique ID"
        // which is ambiguous as regards the message type
        // we will thus call this message Greetings
        // no need to protect for parallel access before we start the other threads
        sendQueue.messages.push({ id, Message::Type::Greetings, "" });

        listenThread = std::make_shared<thread>(&Node::listen, this);
        std::this_thread::sleep_for(1000ms);

        talkThread = std::make_shared<thread>(&Node::talk, this);
        while (finished == false)
        {
            sleep();
            ostringstream s;
            s << "\tprocess\t";

            // s << &sendQueue.messages;
            // PrintSafely(s.str());
            auto stateDescription =
                string{"participating:"} +
                ((state == State::Participating) ? "yes" : "no");

            if (auto optionalMsg = receiveQueue.pop())
            {
                auto msg = *optionalMsg;
                auto actionDescription = string{};

                if (msg.type == Message::Type::Greetings)
                {
                    if (msg.id != id)
                    {
                        peers.insert(msg.id);
                        actionDescription += "forwarding";
                        sendQueue.push(msg);
                    }
                    else
                    {
                        allReady = true;
                        actionDescription += "noop";
                        // PrintSafely("greetings size "  + to_string(peers.size()));
                    }
                }
                else if (msg.type == Message::Type::ElectionStart)
                {
                    if (msg.id > id)
                    {
                        // unconditionally forward
                        state = State::Participating;
                        actionDescription += "forwarding";
                        sendQueue.push(msg);
                    }
                    else if (msg.id < id)
                    {
                        if (state != State::Participating)
                        {
                            // replace the UID in the message with my own UID and send
                            state = State::Participating;
                            msg.id = id;
                            actionDescription += "forwarding with my id";
                            sendQueue.push(msg);
                        }
                        else
                        {
                            // discard the election message
                            actionDescription += "noop";
                        }
                    }
                    else
                    {
                        // i am the leader
                        assert(msg.id == id);
                        leader = id;
                        state = State::Leader;
                        actionDescription += "sending i am the leader";
                        sendQueue.push({ id, Message::Type::ElectedLeader, "" });
                    }
                }
                else if (msg.type == Message::Type::ElectedLeader)
                {
                    if (msg.id != id)
                    {
                        // marks myself as a decided, record the elected UID, and forward
                        state = State::Decided;
                        leader = msg.id;
                        actionDescription += "forwarding";
                        sendQueue.push(msg);
                    }
                    else
                    {
                        actionDescription += "noop";
                        // election is over
                    }
                    assert(leader);
                    finished = true;
                }
                printMessage(msg, Direction::Receive, stateDescription + ", " + actionDescription);
                if (finished)
                {
                    PrintSafely("OUR LEADER IS " + to_string(*leader));
                }
            }

            if (state == State::Offline
                && allReady // waiting for my peers to be ready
                )
            {
                /* no need to wait a grace period
                   because we know when all peers are ready
                auto secondsSinceStart = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now() - startTime).count();
                if (secondsSinceStart > 5)
                */
                {
                    // we notice the lack of a leader
                    state = State::Participating;
                    PrintSafely("noticed lack of a leader, " + stateDescription + ", starting an election");
                    sendQueue.push(Message{id, Message::Type::ElectionStart, "something"});
                }
            }
        }
    }
};
mutex Node::printLock;
string const Node::outputPath = "output.log";

auto generateNodes(vector<float> const& delays)
{
    vector<Node> nodes;
    for (auto const& delay: delays)
    {
        nodes.emplace_back(delay);
    }
    for (size_t i=0; i<nodes.size(); ++i)
    {
        cout <<
            std::setfill('0') << std::setw(5) <<
            nodes[i].getID() << "\t";
        if (i == 0)
        {
            cout << "↖";
        }
        else if (i == nodes.size() - 1)
        {
            cout << "↗";
        }
        else
        {
            cout << "↑";
        }
        cout << endl;
        if (i != nodes.size() - 1)
        {
            cout << "↓" << endl;
        }
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

void startNodes(vector<Node>& nodes)
{
    // clear the log file
    try
    {
        filesystem::remove(Node::outputPath);
    }
    catch (std::exception const&) { }

    // link to neighbor and start processing, listening and talking
    for (size_t i=0; i<nodes.size(); ++i)
    {
        // std::cout << "index " << i << " neighbor " << (i + nodes.size() - 1) % nodes.size() << std::endl;
        auto const& counterClockwiseNeighbor =
            nodes.at((i + nodes.size() - 1) % nodes.size());
        nodes[i].linkAndStart(counterClockwiseNeighbor.getPort());
    }
    for (auto const& node: nodes)
    {
        node.printDescription();
    }
}

void endWork(vector<Node>& nodes)
{
    while (std::any_of(nodes.begin(), nodes.end(), [] (Node const& node) {
        return node.getFinished() == false; }))
    {
        std::this_thread::sleep_for(100ms);
    }
    for (auto& node: nodes)
    {
        node.join();
    }

    auto leader = nodes.front().getLeader();
    if (std::any_of(nodes.begin(), nodes.end(), [&leader, &nodes] (Node const& node) {
        if (!node.getLeader())
        {
            cout << "Leader: none for id " << node.getID() << endl;
            return true;
        }
        if (node.getLeader() != leader)
        {
            cout << "Leaders differ: " << to_string(*node.getLeader()) << " for " << to_string(node.getID()) <<
                " and " << (leader ? to_string(*leader) : "none") << " for " << to_string(nodes.front().getID()) <<
                endl;
        }
        return false; }))
    {
        throw std::runtime_error("No consensus");
    }
}

void unitTestJson()
{
    vector<json::Message::Type> types =
        {
            json::Message::Type::Greetings,
            json::Message::Type::ElectionStart,
            json::Message::Type::ElectedLeader
        };
    for (auto type: types)
    {
        json::Message m = { 5584, type, "iorjjkgfd" };
        auto s = json::to_string(m);
        cout << endl << s << endl;
        auto m2 = json::from_string(s);
        cout << endl << (m2 ? "parsed" : "empty") << endl;
        if (m2)
        {
            cout << endl << json::to_string(*m2) << endl;
        }
    }
}

void printCollisionProbability()
{
    auto probabilityOfACollision = [] (int numberOfNodes) {
        auto const totalIDs = 65536;
        auto remainingIDs = totalIDs;
        auto probaNoCollision = 1.0f;

        for (int i=0; i<numberOfNodes; ++i)
        {
            probaNoCollision *= (float)remainingIDs / totalIDs;
            if (probaNoCollision < 0.00001)
            {
                break;
            }
            --remainingIDs;
        }
        return 1 - probaNoCollision;
    };

    for (int i=0; i<=65536; ++i)
    {
        std::cout <<
            "probability of a collision for " << i << " nodes = \t" <<
            std::fixed <<
            std::setprecision(2) <<
            probabilityOfACollision(i) * 100 << "%" << endl;
    }
}

int main(int argc, char** argv)
{
    // printCollisionProbability();
    // return 0;

    // unitTestJson();
    // return 0;

    auto inputFile = parseCommandLine(argc, argv);

    auto delays = parseInput(inputFile);

    auto nodes = generateNodes(delays);

    verifyUniqueIDs(nodes);

    startNodes(nodes);

    endWork(nodes);
    std::cout << "end main()" << std::endl;

    return 0;
}
