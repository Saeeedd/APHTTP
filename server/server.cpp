#include "server.hpp"
#include "../utils/utilities.hpp"

class Exception : public std::exception {
public:
  Exception() {}
  Exception(const char *pStr) {}
  const char *getMessage();

private:
  const char *pMessage;
};

void split(string str, string separator, int max, vector<string> *results) {
  int i = 0;
  size_t found = str.find_first_of(separator);

  while (found != string::npos) {
    if (found > 0)
      results->push_back(str.substr(0, found));
    str = str.substr(found + 1);
    found = str.find_first_of(separator);

    if (max > -1 && ++i == max)
      break;
  }
  if (str.length() > 0)
    results->push_back(str);
}

Request *parse_headers(char *headers) {
  Request *req;
  int i = 0;
  char *pch;
  for (pch = strtok(headers, "\n"); pch; pch = strtok(NULL, "\n")) {
    if (i++ == 0) {
      vector<string> R;
      string line(pch);

      split(line, " ", 3, &R);
      if (R.size() != 3) {
        // throw error
      }
      req = new Request(R[0]);
      req->setPath(R[1]);
      size_t pos = req->getPath().find('?');
      if (pos != string::npos) {
        vector<string> Q1;
        split(req->getPath().substr(pos + 1), "&", -1, &Q1);
        for (vector<string>::size_type q = 0; q < Q1.size(); q++) {
          vector<string> Q2;
          split(Q1[q], "=", -1, &Q2);
          if (Q2.size() == 2)
            req->setQueryParam(Q2[0], Q2[1]);
        }
        req->setPath(req->getPath().substr(0, pos));
      }
    } else {
      vector<string> R;
      string line(pch);
      split(line, ": ", 2, &R);
      if (R.size() == 2) {
        req->setHeader(R[0], R[1]);
      }
      vector<string> body;
      split(line, "&", 10, &body);
      if (body.size() > 1) {
        for (size_t i = 0; i < body.size(); i++) {
          vector<string> field;
          split(body[i], "=", 2, &field);
          req->setBodyParam(field[0], field[1]);
        }
      }
    }
  }
  return req;
}

string charArrayToString(char *arr, int size) {
  string result = "";
  for (int i = 0; i < size; i++)
    result += arr[i];
  return result;
}

void parseBody(Request *req, string line) {
  vector<string> body;
  split(line, "&", 10, &body);
  if (body.size() > 1) {
    for (size_t i = 0; i < body.size(); i++) {
      vector<string> field;
      split(body[i], "=", 2, &field);
      req->setBodyParam(field[0], field[1]);
    }
  }
}

Route::Route(Method _method, string _path) {
  method = _method;
  path = _path;
}

bool Route::isMatch(Method _method, string url) {
  return (url == path) && (_method == method);
}

Response *Route::handle(Request *req) { return handler->callback(req); }

Server::Server(int _port) { port = _port; }

void Server::get(string path, RequestHandler *handler) {
  Route *route = new Route(GET, path);
  route->handler = handler;
  routes.push_back(route);
}

void Server::post(string path, RequestHandler *handler) {
  Route *route = new Route(POST, path);
  route->handler = handler;
  routes.push_back(route);
}

class NotFoundHandler : public RequestHandler {
public:
  Response *callback(Request *req) {
    Response *res = new Response;
    res->setBody(readFile("htmlFiles/404.html"));
    res->setHeader("Content-Type", "text/html");
    res->setStatus(404, "Not Found");
    return res;
  }
};

void Server::run() {
  sc = socket(AF_INET, SOCK_STREAM, 0);
  if (sc < 0)
    throw Exception("Error opening socket");
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  if (::bind(sc, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
    throw Exception("Error on binding");

  listen(sc, 5);

  RequestHandler *notFoundHandler = new NotFoundHandler();
  struct sockaddr_in cli_addr;
  socklen_t clilen;
  clilen = sizeof(cli_addr);
  int newsc;
  while (true) {
    newsc = accept(sc, (struct sockaddr *)&cli_addr, &clilen);
    if (newsc < 0)
      throw Exception("Error on accept");
    char headers[BUFSIZE + 1];

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(newsc, &fds);
    long ret = read(newsc, headers, BUFSIZE);
    // cout << charArrayToString(headers, ret) << endl;
    Request *req = parse_headers(headers);
    int cl = 0;
    if (req->getHeader("Content-Length") != "")
      stoi(req->getHeader("Content-Length"));
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    int rc = select(sizeof(fds) * 8, &fds, NULL, NULL, &timeout);
    if (rc) {
      char body[cl + 1];
      ret = read(newsc, body, cl);
      parseBody(req, charArrayToString(body, cl));
    }
    Response *res = new Response();
    size_t i = 0;
    for (; i < routes.size(); i++) {
      if (routes[i]->isMatch(req->getMethod(), req->getPath())) {
        res = routes[i]->handle(req);
        break;
      }
    }
    if (i == routes.size()) {
      res = notFoundHandler->callback(req);
    }
    char *header_buffer = res->print();
    ssize_t t = write(newsc, header_buffer, strlen(header_buffer));
  }
}

Response *RequestHandler::callback(Request *req) { return NULL; }