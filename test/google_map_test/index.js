const api_host = "http://127.0.0.1:9002/can-data";
let map;

// The fetchData function remains the same
async function getGarbageCanList(url) {
  try {
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error('Network response was not ok');
    }
    const data = await response.json();
    return data;
  } catch (error) {
    console.error('There was a problem with the fetch operation:', error);
    throw error;
  }
}

async function initMap() {
  // Request needed libraries.
  const { Map } = await google.maps.importLibrary("maps");
  const { AdvancedMarkerElement, PinElement } = await google.maps.importLibrary(
    "marker",
  );

  map = new google.maps.Map(document.getElementById("map"), {
    center: new google.maps.LatLng(23.76170563, 120.6814866),
    zoom: 16,
    mapId: "DEMO_MAP_ID",
  });

  const iconBase =
    "https://developers.google.com/maps/documentation/javascript/examples/full/images/";
  const icons = {
    parking: {
      icon: iconBase + "parking_lot_maps.png",
    },
    library: {
      icon: iconBase + "library_maps.png",
    },
    info: {
      // icon: iconBase + "info-i_maps.png",
      icon: "https://cdn-icons-png.flaticon.com/256/4231/4231212.png"
    },
  };

  const mapCanList = [];
  const apiCanList = await getGarbageCanList(api_host);
  console.log(apiCanList);
  for (const can of apiCanList) {
    const id = can[0];
    const name = can[1];
    const lat = +can[2];
    const lng = +can[3];
    const capacity = +can[4];
    mapCanList.push({
      position: new google.maps.LatLng(lat, lng),
      type: "info",
      id: id,
      name: name,
      capacity: capacity
    });
  }


  for (let i = 0; i < mapCanList.length; i++) {
    const iconImage = document.createElement("img");
    iconImage.style.maxWidth = "40px"
    iconImage.style.maxHeight  = "40px"

    iconImage.src = icons[mapCanList[i].type].icon;

    const contentString =
    `
    <table border="1">
      <thead>
          <tr>
            <th>ID</th>
            <th>名稱</th>
            <th>容量</th>
        </tr>
      </thead>
      <tbody>
        <tr>
          <td>${mapCanList[i].id}</td>
          <td>${mapCanList[i].name}</td>
          <td>${mapCanList[i].capacity}</td>
        </tr>
      </tbody>
    </table>
    `;

    const infowindow = new google.maps.InfoWindow({
      content: contentString,
      ariaLabel: "Uluru",
    });

    const marker = new google.maps.marker.AdvancedMarkerElement({
      map,
      position: mapCanList[i].position,
      title: mapCanList[i].name,
      content: iconImage,
    });

    marker.addListener("click", () => {
      infowindow.open({
        anchor: marker,
        map,
      });
    });
  }
}

initMap();
