#include <iostream>
#include <vector>
#include <utility>
#include "ionex.hpp"

using namespace ngpt;
typedef std::pair<float, float> point;

int main(int argc, char* argv[])
{
    // must pass the atx file to inspect as cmd
    if ( argc != 2 ) {
        std::cout << "\nUsage: testIonex <inxfile>\n";
        return 1;
    }

    // header is read during construction.
    // this may throw in DEBUG mode; nevermind let it BOOM
    ionex inx ( argv[1] );
    std::cout << "\nIONEX header read ok!";

    std::vector<point> pts;
    std::vector<ionex::datetime_ms> epochs;
    pts.emplace_back(23.68, 32.14);

    auto tec_vals = inx.get_tec_at( pts, epochs );

    std::cout << "\n";
    return 0;
}
