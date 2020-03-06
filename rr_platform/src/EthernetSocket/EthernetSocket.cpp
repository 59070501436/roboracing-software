#include <rr_platform/EthernetSocket.h>
#include <iostream>

using boost::asio::ip::tcp;

rr::EthernetSocket::EthernetSocket(std::string ip_addr, int port)
{
  // resolve all possible endpoints
  tcp::resolver resolver(io_service_);
  tcp::resolver::query query(ip_addr, std::to_string(port));
  tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

  // look through endpoints and hit socket's connect() member function until
  // a successful TCP connection is established
  this->sock_ = std::make_unique<tcp::socket>(io_service_);
  boost::asio::connect(*sock_, endpoint_iterator);
}

rr::EthernetSocket::~EthernetSocket()
{
  // shut down the TCP connection
  this->sock_->shutdown(boost::asio::ip::tcp::socket::shutdown_send);
}

void rr::EthernetSocket::sendMessage(char* message, size_t len)
{
  boost::system::error_code error;
  // Create boost buffer from string and send to TCP endpoint
  boost::asio::write(*sock_, boost::asio::buffer(message, len), error);
}

void rr::EthernetSocket::sendMessage(std::string message)
{
  boost::system::error_code error;
  // Create boost buffer from string and send to TCP endpoint
  boost::asio::write(*sock_, boost::asio::buffer(message), error);
}

size_t rr::EthernetSocket::readMessage(unsigned char (&buffer)[256])
{
  // read data from TCP connection
  boost::system::error_code error;

  size_t len = sock_->read_some(boost::asio::buffer(buffer, sizeof(buffer) - 1), error);

  if (error == boost::asio::error::eof)  // connection closed by server
    len = 0;
  else if (error)
    throw boost::system::system_error(error);

  return len;
}

size_t rr::EthernetSocket::readMessage(boost::array<char, 128> buf)
{
  // read data from TCP connection
  boost::system::error_code error;

  size_t len = sock_->read_some(boost::asio::buffer(buf), error);

  if (error == boost::asio::error::eof)  // connection closed by server
    len = 0;
  else if (error)
    throw boost::system::system_error(error);

  return len;
}

std::string rr::EthernetSocket::getIP()
{
  return sock_->remote_endpoint().address().to_string();
}

int rr::EthernetSocket::getPort()
{
  return static_cast<int>(sock_->remote_endpoint().port());
}

std::string rr::EthernetSocket::getBoostVersion()
{
  std::stringstream version;
  version << BOOST_VERSION / 100000 << "."      // major version
          << BOOST_VERSION / 100 % 1000 << "."  // minor version
          << BOOST_VERSION % 100;               // patch level

  return version.str();
}
