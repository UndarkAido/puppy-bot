#include "include.hh"

namespace asio = boost::asio;
using json = nlohmann::json;
namespace dpp = discordpp;
using s_clock = std::chrono::system_clock;
using namespace std::literals::chrono_literals;

const auto tz = -6h;

std::string getToken();

std::istream &safeGetline(std::istream &is, std::string &t);

void filter(std::string &target, const std::string &pattern);

int main() {
    dpp::log::filter = dpp::log::info;
    dpp::log::out = &std::cerr;

    std::cout << "Howdy, and thanks for trying out Discord++!\n"
              << "Feel free to drop into the official server at "
                 "https://discord.gg/VHAyrvspCx if you have any questions.\n\n"
              << std::flush;

    std::cout << "Starting bot...\n\n";

    std::string token = getToken();
    if (token.empty()) {
        std::cerr << "CRITICAL: "
                  << "There is no valid way for Echo to obtain a token! Use "
                     "one of the following ways:"
                  << std::endl
                  << "(1) Fill the BOT_TOKEN environment variable with the "
                     "token (e.g. 'Bot 123456abcdef')."
                  << std::endl
                  << "(2) Copy the example `token.eg.dat` as `token.dat` and "
                     "write your own token to it.\n";
        exit(1);
    }

    // Create Bot object
    auto bot = newBot();

    // Create Asio context, this handles async stuff.
    auto aioc = std::make_shared<asio::io_context>();

    // A timer for recurring reminders
    std::unique_ptr<boost::asio::system_timer> reminder;

    // Don't complain about unhandled events
    bot->debugUnhandled = false;

    // Declare the intent to receive guild messages
    // You don't need `NONE` it's just to show you how to declare multiple
    bot->intents =
        dpp::intents::GUILD_MESSAGES | dpp::intents::GUILD_MESSAGE_REACTIONS;

    /*/
     * Create handler for the READY payload, this may be handled by the bot in
    the future.
     * The `self` object contains all information about the 'bot' user.
    /*/
    json self;
    bot->handlers.insert(
        {"READY", [&self](json data) { self = data["user"]; }});

    bot->prefix = "!";

    const std::string helpSchedule =
        "`" + bot->prefix +
        "schedule TIME INTERVAL [USERS...]` Set up recurring puppy reminders\n"
        "\t`TIME` is formatted `HH:MM`\n"
        "\t`INTERVAL` is formatted `H:MM`\n"
        "\t`USERS...` is any number of users to mention including zero\n";

    std::ostringstream help;
    help << "Puppybot is a bot to help you take care of your new puppy!\n"
         << "`" << bot->prefix << "help` Print this message\n"
         << helpSchedule;

    bot->respond("help", help.str());

    std::function<void(
        const boost::system::error_code ec, const s_clock::time_point triggered,
        const std::chrono::minutes period, const std::string &channel_id,
        const std::string &targets)>
        reminder_f;
    reminder_f = [aioc, bot, &reminder,
                  &reminder_f](const boost::system::error_code ec,
                               const s_clock::time_point triggered,
                               const std::chrono::minutes period,
                               const std::string &channel_id,
                               const std::string &targets) {
        if (ec.failed()) {
            return;
        }

        bot->call("POST", "/channels/" + channel_id + "/messages",
                  json({{"content", ":bone: **IT'S PUPPY TIME!** " + targets}}));

        reminder = std::make_unique<boost::asio::system_timer>(
            *aioc, s_clock::now() + period); //+ std::chrono::minutes(1)
        reminder->async_wait([&reminder_f, triggered, period, channel_id,
                              targets](const boost::system::error_code ec) {
            reminder_f(ec, triggered + period, period, channel_id, targets);
        });
    };

    bot->respond("schedule", [bot, &helpSchedule, aioc, &reminder,
                              &reminder_f](const json &msg) {
        auto argc = msg["content"].get<std::string>();
        std::vector<std::string> argv;
        for (size_t pos; (pos = argc.find(' ')) != std::string::npos;) {
            std::string arg = argc.substr(0, pos);
            argv.push_back(arg);
            argc.erase(0, pos + 1);
        }
        argv.push_back(argc);

        for (const auto &arg : argv) {
            std::cout << arg << std::endl;
        }

        try {
            if (argv.size() < 3) {
                throw std::logic_error("Wrong arg count");
            }

            if (argv[1].at(1) == ':') {
                argv[1] = '0' + argv[1];
            }

            std::ostringstream out;

            s_clock::time_point now = s_clock::now();
            s_clock::time_point today =
                std::chrono::floor<std::chrono::days>(now + tz) - tz;
            // auto todaymin =
            // std::chrono::time_point_cast<std::chrono::minutes>(std::chrono::floor<std::chrono::days>(now));

            std::time_t now_tt = s_clock::to_time_t(now);
            std::time_t today_tt = s_clock::to_time_t(today);

            std::tm now_local = *std::localtime(&now_tt);
            std::tm now_gm = *std::gmtime(&now_tt);
            std::tm today_local = *std::localtime(&today_tt);
            std::tm today_gm = *std::gmtime(&today_tt);

            /*out << "`Now,  Local:  ` " << std::put_time(&now_local, "%F %T")
                << '\n';
            out << "`Now, GMTime:  ` " << std::put_time(&now_gm, "%F %T")
                << '\n';
            out << "`Today,  Local:` " << std::put_time(&today_local, "%F %T")
                << '\n';
            out << "`Today, GMTime:` " << std::put_time(&today_gm, "%F %T")
                << '\n';*/

            // s_clock::time_point start =
            // s_clock::from_time_t(std::mktime(&start_tm));

            s_clock::time_point start;
            {
                std::stringstream ss;
                ss.exceptions(std::ios::failbit | std::ios::badbit);
                ss << std::put_time(&today_local, "%F ") << argv[1];
                ss >> date::parse("%Y-%m-%d %H:%M", start);
            }
            start -= tz;

            std::cout << date::format("%c", start + tz) << std::endl;
            std::cout << date::format("%c", now + tz) << std::endl;

            if (start < now) {
                start += 24h;
                std::cout << date::format("%c", start + tz) << std::endl;
                std::cout << date::format("%c", now + tz) << std::endl;
                if (start < now) {
                    throw std::logic_error("Invalid start time");
                }
            }

            out << ":dog: Puppy alarms will start at "
                << date::format("%I:%M %p on %A", start + tz) << '\n';

            std::chrono::minutes period;
            {
                size_t sep = argv[2].find(':');
                period = std::chrono::hours(std::stoi(argv[2].substr(0, sep))) +
                         std::chrono::minutes(std::stoi(
                             argv[2].substr(sep + 1, argv[2].size())));
            }

            if (period <= std::chrono::minutes::zero()) {
                throw std::logic_error("Negative or zero period");
            }

            {
                out << "They'll recur every ";
                auto hours =
                    std::chrono::floor<std::chrono::hours>(period).count();
                auto minutes = std::chrono::minutes(period).count() % 60;
                if (hours > 0) {
                    out << hours << " hour" << (hours == 1 ? "" : "s");
                }
                if (hours > 0 && minutes > 0) {
                    out << " and ";
                }
                if (minutes > 0) {
                    out << minutes << " minute" << (minutes == 1 ? "" : "s");
                }
                out << "\n";
            }

            bot->call("POST",
                      "/channels/" + msg["channel_id"].get<std::string>() +
                          "/messages",
                      json({{"content", out.str()}}));

            {
                std::string channel_id = msg["channel_id"],
                            guild_id = msg["guild_id"], targets;
                if (argv.size() > 3) {
                    bool first = true;
                    std::for_each(argv.begin() + 3, argv.end(),
                                  [&targets, &first](std::string target) {
                                      if (first) {
                                          first = false;
                                      } else {
                                          targets += " ";
                                      }
                                      targets += target;
                                  });
                } else {
                    targets = "@everyone";
                }
                reminder =
                    std::make_unique<boost::asio::system_timer>(*aioc, start);
                reminder->async_wait(
                    [&reminder_f, start, period, channel_id, guild_id,
                     targets](const boost::system::error_code ec) {
                        reminder_f(ec, start, period, channel_id, targets);
                    });
            }
        } catch (...) {
            bot->call("POST",
                      "/channels/" + msg["channel_id"].get<std::string>() +
                          "/messages",
                      json({{"content", helpSchedule}}));
        }
    });

    // Set the bot up
    bot->initBot(6, token, aioc);

    // Run the bot!
    bot->run();

    return 0;
}

std::string getToken() {
    std::string token;

    /*
                    First attempt to read the token from the BOT_TOKEN
       environment variable.
    */
    char const *env = std::getenv("BOT_TOKEN");
    if (env != nullptr) {
        token = std::string(env);
    } else {
        /*/
         * Read token from token file.
         * Tokens are required to communicate with Discord, and hardcoding
        tokens is a bad idea.
         * If your bot is open source, make sure it's ignore by git in your
        .gitignore file.
        /*/
        std::ifstream tokenFile("token.dat");
        if (!tokenFile) {
            return "";
        }
        safeGetline(tokenFile, token);
        tokenFile.close();
    }
    return token;
}

/*/
 * Source: https://stackoverflow.com/a/6089413/1526048
/*/
std::istream &safeGetline(std::istream &is, std::string &t) {
    t.clear();

    // The characters in the stream are read one-by-one using a
    // std::streambuf. That is faster than reading them one-by-one using the
    // std::istream. Code that uses streambuf this way must be guarded by a
    // sentry object. The sentry object performs various tasks, such as
    // thread synchronization and updating the stream state.

    std::istream::sentry se(is, true);
    std::streambuf *sb = is.rdbuf();

    for (;;) {
        int c = sb->sbumpc();
        switch (c) {
        case '\n':
            return is;
        case '\r':
            if (sb->sgetc() == '\n') {
                sb->sbumpc();
            }
            return is;
        case std::streambuf::traits_type::eof():
            // Also handle the case when the last line has no line ending
            if (t.empty()) {
                is.setstate(std::ios::eofbit);
            }
            return is;
        default:
            t += (char)c;
        }
    }
}
