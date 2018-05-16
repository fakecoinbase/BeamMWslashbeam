#include "wallet/wallet_network.h"
#include "core/common.h"

#include "node.h"
#include "wallet/wallet.h"
#include "wallet/keychain.h"
#include "wallet/wallet_network.h"
#include "core/ecc_native.h"
#include "utility/logger.h"

#include <boost/program_options.hpp>

namespace po = boost::program_options;
using namespace std;
using namespace beam;

static void printHelp(const po::options_description& options)
{
    cout << options << std::endl;
}

int main(int argc, char* argv[])
{
    LoggerConfig lc;
    int logLevel = LOG_LEVEL_DEBUG;
#if LOG_VERBOSE_ENABLED
    logLevel = LOG_LEVEL_VERBOSE;
#endif
    lc.consoleLevel = logLevel;
    lc.flushLevel = logLevel;
    auto logger = Logger::create(lc);

    po::options_description general_options("General options");
    general_options.add_options()
        ("help,h", "list of all options")
        ("mode", po::value<string>()->required(), "mode to execute [node|wallet]")
        ("port,p", po::value<uint16_t>()->default_value(10000), "port to start the server on");

    po::options_description node_options("Node options");
    node_options.add_options()
        ("storage", po::value<string>()->default_value("node.db"), "node storage path");

    po::options_description wallet_options("Wallet options");
    wallet_options.add_options()
        ("pass", po::value<string>()->default_value(""), "password for the wallet")
        ("amount,a", po::value<ECC::Amount>(), "amount to send")
        ("receiver_addr,r", po::value<string>(), "address of receiver")
        ("node_addr,n", po::value<string>(), "address of node")
        ("command", po::value<string>(), "command to execute [send|listen|init|init-debug]");

    po::options_description options{ "Allowed options" };
    options.add(general_options)
           .add(node_options)
           .add(wallet_options);

    po::positional_options_description pos;
    pos.add("mode", 1);

    try
    {
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv)
            .options(options)
            .positional(pos)
            .run(), vm);

        if (vm.count("help"))
        {
            printHelp(options);

            return 0;
        }

        po::notify(vm);

        auto port = vm["port"].as<uint16_t>();

        if (vm.count("mode"))
        {
            io::Reactor::Ptr reactor(io::Reactor::create());
            io::Reactor::Scope scope(*reactor);
            auto mode = vm["mode"].as<string>();
            if (mode == "node")
            {
                beam::Node node;

                node.m_Cfg.m_Listen.port(port);
                node.m_Cfg.m_Listen.ip(INADDR_ANY);
                node.m_Cfg.m_sPathLocal = vm["storage"].as<std::string>();

                LOG_INFO() << "starting a node on " << node.m_Cfg.m_Listen.port() << " port...";

                node.Initialize();
                reactor->run();
            }
            else if (mode == "wallet")
            {
                if (vm.count("command"))
                {
                    auto command = vm["command"].as<string>();
                    if (command != "init" && command != "init-debug" && command != "send" && command != "listen")
                    {
                        LOG_ERROR() << "unknown command: \'" << command << "\'";
                        return -1;
                    }

                    LOG_INFO() << "starting a wallet...";

                    std::string pass(vm["pass"].as<std::string>());
                    if (!pass.size())
                    {
                        LOG_ERROR() << "Please, provide password for the wallet.";
                        return -1;
                    }

                    if (command == "init")
                    {
                        auto keychain = Keychain::init(pass);

                        if (keychain)
                        {
                            LOG_INFO() << "wallet successfully created...";
                            return 0;
                        }
                        else
                        {
                            LOG_ERROR() << "something went wrong, wallet not created...";
                            return -1;
                        }
                    }

                    if (command == "init-debug")
                    {
                        auto keychain = Keychain::initDebug(pass);

                        if (keychain)
                        {
							keychain->store(Coin(keychain->getNextID(), 5));
							keychain->store(Coin(keychain->getNextID(), 10));
							keychain->store(Coin(keychain->getNextID(), 20));
							keychain->store(Coin(keychain->getNextID(), 50));
							keychain->store(Coin(keychain->getNextID(), 100));
							keychain->store(Coin(keychain->getNextID(), 200));
							keychain->store(Coin(keychain->getNextID(), 500));

                            LOG_INFO() << "wallet with coins successfully created...";
                            return 0;
                        }
                        else
                        {
                            LOG_ERROR() << "something went wrong, wallet not created...";
                            return -1;
                        }
                    }

                    auto keychain = Keychain::open(pass);
                    if (!keychain)
                    {
                        LOG_ERROR() << "something went wrong, wallet not opened...";
                        return -1;
                    }

                    LOG_INFO() << "wallet sucessfully opened...";

                    // resolve address after network io
                    io::Address node_addr;
                    node_addr.resolve(vm["node_addr"].as<string>().c_str());
                    bool is_server = command == "listen";
                    WalletNetworkIO wallet_io{ io::Address::localhost().port(port)
                                             , node_addr
                                             , is_server
                                             , keychain
                                             , reactor};

                    if (command == "send")
                    {
                        auto amount = vm["amount"].as<ECC::Amount>();
                        io::Address receiver_addr;
                        receiver_addr.resolve(vm["receiver_addr"].as<string>().c_str());
                    
                        LOG_INFO() << "sending money " << receiver_addr.str();
                        wallet_io.send_money(receiver_addr, move(amount));
                    }
                    wallet_io.start();
                }
                else
                {
                    LOG_ERROR() << "command parameter not specified.";
                    printHelp(options);
                }
            }
            else
            {
                LOG_ERROR() << "unknown mode \'" << mode << "\'.";
                printHelp(options);
            }
        }
    }
    catch(const po::error& e)
    {
        LOG_ERROR() << e.what();
        printHelp(options);
    }

    return 0;
}
