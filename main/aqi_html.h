#define AQI_HTML \
  "<!DOCTYPE html>" \
  "<html>" \
    "<head>" \
      "<title>AQI Predictions</title>" \
    "</head>" \
    "<body>" \
      "<table>" \
        "<caption>" \
          "AQI of the next 24 hours" \
        "</caption>" \
        "<thead>" \
          "<tr>" \
            "<th scope=\"col\">Time</th>" \
            "<th scope=\"col\">AQI</th>" \
          "</tr>" \
        "</thead>" \
        "<tbody>" \
          "<tr>" \
            "<th scope=\"row\">T+1</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+2</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+3</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+4</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+5</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+6</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+7</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+8</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+9</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+10</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+11</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+12</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+13</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+14</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+15</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+16</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+17</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+18</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+19</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+20</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+21</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+22</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+23</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
          "<tr>" \
            "<th scope=\"row\">T+24</th>" \
            "<td>%.0f</td>" \
          "</tr>" \
        "</tbody>" \
      "</table>" \
    "</body>" \
  "</html>"

#define UNPACK_24(arr) (arr)[0], (arr)[1], (arr)[2], (arr)[3], (arr)[4], \
  (arr)[5], (arr)[6], (arr)[7], (arr)[8], (arr)[9], (arr)[10], (arr)[11], \
  (arr)[12], (arr)[13], (arr)[14], (arr)[15], (arr)[16], (arr)[17], (arr)[18], \
  (arr)[19], (arr)[20], (arr)[21], (arr)[22], (arr)[23]
