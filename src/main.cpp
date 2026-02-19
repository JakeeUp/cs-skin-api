#include "crow_all.h"

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](){
        return "CS Skin API is running!";
    });

    CROW_ROUTE(app, "/health")([](){
        crow::json::wvalue response;
        response["status"] = "ok";
        response["message"] = "CS Skin API is alive";
        return response;
    });

    app.port(8080).multithreaded().run();
}