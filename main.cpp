#include <iostream>
#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <pqxx/pqxx>
#include <boost/beast/core/detail/base64.hpp>

namespace asio = boost::asio;
using asio::ip::tcp;
using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;

const int PORT = 12345;
const std::string DB_CONNECTION_STRING = "dbname user password host";

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void Start() {
        ReadData();
    }

private:
    void ReadData() {
        auto self(shared_from_this());
        asio::async_read_until(socket_, buffer_, "\n",
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::istream is(&buffer_);
                    std::string json_str;
                    std::getline(is, json_str);

                    try {
                        ProcessJson(json_str);
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Error processing JSON: " << e.what() << std::endl;
                        SendResponse("ERROR: " + std::string(e.what()));
                    }
                }
            });
    }

    void ProcessJson(const std::string& json_str) {
        std::stringstream ss(json_str);
        ptree pt;
        read_json(ss, pt);

        std::string username, password;
        try {
            username = pt.get<std::string>("Data.Users.Credentials.username");
            password = pt.get<std::string>("Data.Users.Credentials.pass");
            password = Base64Encode(password);
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Invalid JSON structure: missing required fields");
        }

        pqxx::connection conn(DB_CONNECTION_STRING);
        pqxx::work txn(conn);

        try {
            pqxx::result res = txn.exec_params(
                "SELECT id FROM users WHERE username = $1 AND password = $2",
                username, password);

            if (res.empty()) {
                CreateNewUser(txn, pt);
                SendResponse("SUCCESS: New user created");
            }
            else {
                int user_id = res[0][0].as<int>();
                std::string user_info = GetUserInfo(txn, user_id);
                SendResponse("SUCCESS: " + user_info);
            }

            txn.commit();
        }
        catch (const std::exception& e) {
            txn.abort();
            throw std::runtime_error("Database error: " + std::string(e.what()));
        }
    }

    void CreateNewUser(pqxx::work& txn, const ptree& pt) {
        int id = pt.get<int>("id");
        std::string referralGUID = pt.get<std::string>("referralGUID");
        std::string referralDate = pt.get<std::string>("referralDate");

        const ptree& user = pt.get_child("Data.Users");
        std::string lastName = user.get<std::string>("lastName");
        std::string firstName = user.get<std::string>("firstName");
        std::string patrName = user.get<std::string>("patrName");
        std::string birthDate = user.get<std::string>("birthDate");
        std::string sex = user.get<std::string>("sex");
        std::string phoneNumber = user.get<std::string>("phoneNumber");
        std::string snils = user.get<std::string>("snils");
        std::string inn = user.get<std::string>("inn");
        std::string socStatus_id = user.get<std::string>("socStatus_id");
        std::string socStatusFed_Code = user.get<std::string>("socStatusFed_Code");

        const ptree& credentials = user.get_child("Credentials");
        std::string username = credentials.get<std::string>("username");
        std::string password = Base64Encode(credentials.get<std::string>("pass"));

        const ptree& address = user.get_child("Address");
        std::string address_id = address.get<std::string>("id");
        std::string address_value = address.get<std::string>("value");
        std::string address_guid = address.get<std::string>("guid");

        txn.exec_params(
            "INSERT INTO users (id, referralGUID, referralDate, lastName, firstName, "
            "patrName, birthDate, sex, phoneNumber, snils, inn, socStatus_id, "
            "socStatusFed_Code, username, password, address_id, address_value, address_guid) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18)",
            id, referralGUID, referralDate, lastName, firstName, patrName, birthDate,
            sex, phoneNumber, snils, inn, socStatus_id, socStatusFed_Code,
            username, password, address_id, address_value, address_guid);

        for (const auto& doc_item : user.get_child("Documents")) {
            const ptree& doc = doc_item.second;
            std::string doc_id = doc.get<std::string>("id");
            std::string docType_id = doc.get<std::string>("documentType_id");
            std::string documentType_Name = doc.get<std::string>("documentType_Name");
            std::string series = doc.get<std::string>("series", "");
            std::string number = doc.get<std::string>("number");
            std::string beginDate = doc.get<std::string>("beginDate");
            std::string endDate = doc.get<std::string>("endDate", "");
            std::string orgDep_Name = doc.get<std::string>("orgDep_Name", "");

            txn.exec_params(
                "INSERT INTO user_documents (id, user_id, docType_id, documentType_Name, "
                "series, number, beginDate, endDate, orgDep_Name) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
                doc_id, id, docType_id, documentType_Name, series, number, beginDate, endDate, orgDep_Name);
        }
    }

    std::string GetUserInfo(pqxx::work& txn, int user_id) {
        pqxx::result res = txn.exec_params(
            "SELECT id, firstName, lastName, phoneNumber FROM users WHERE id = $1",
            user_id);

        if (res.empty()) {
            return "User not found";
        }

        std::stringstream ss;
        ss << "User info: ID=" << res[0][0].as<int>()
            << ", Name=" << res[0][1].as<std::string>() << " " << res[0][2].as<std::string>()
            << ", Phone=" << res[0][3].as<std::string>();
        return ss.str();
    }

    void SendResponse(const std::string& response) {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(response + "\n"),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    std::cout << "Response sent successfully" << std::endl;
                }
            });
    }

    tcp::socket socket_;
    asio::streambuf buffer_;
};

class Server {
public:
    Server(asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        DoAccept();
    }

private:
    void DoAccept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->Start();
                }
                DoAccept();
            });
    }

    tcp::acceptor acceptor_;
};

int main() {
    try {
        asio::io_context io_context;
        Server server(io_context, PORT);
        std::cout << "Server started on port " << PORT << std::endl;
        io_context.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
