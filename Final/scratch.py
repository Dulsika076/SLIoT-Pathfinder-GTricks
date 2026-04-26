import sys

with open('d:\\SLIOT\\final\\Final\\templates\\dashboard.html', 'r', encoding='utf-8') as f:
    content = f.read()

start_marker = '<!-- 3D Scene — runs after Three.js CDN onload calls init3D() -->'
end_marker = 'console.log(\'✅ PATHFINDER 3D terrain loaded — Three.js r128 OK\');\n}\n</script>'

if start_marker in content and end_marker in content:
    start_idx = content.find(start_marker)
    end_idx = content.find(end_marker) + len(end_marker)
    
    new_script = """<!-- 3D Scene / MapLibre / Leaflet -->
<script>
function initMap(){
  var centerCoords = [80.7347, 6.7894]; /* Knuckles range [lng, lat] */
  
  // Create Beacon HTML Element
  var beaconEl = document.createElement('div');
  beaconEl.className = 'beacon-container';
  beaconEl.innerHTML = '<div class="beacon-ring"></div><div class="beacon-ring"></div><div class="beacon-ring"></div><div class="beacon-core"></div>';
  window._beaconEl = beaconEl;

  var isWebGLSupported = false;
  try {
    var canvas = document.createElement('canvas');
    isWebGLSupported = !!(window.WebGLRenderingContext && 
      (canvas.getContext('webgl') || canvas.getContext('experimental-webgl')));
  } catch(e) {}

  if(isWebGLSupported && window.maplibregl) {
    console.log('✅ WebGL supported. Initialising MapLibre 3D Terrain.');
    var map = new maplibregl.Map({
      container: 'map',
      style: {
        version: 8,
        sources: {
          'esri-satellite': {
            type: 'raster',
            tiles: ['https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}'],
            tileSize: 256,
            attribution: 'Esri Satellite'
          },
          'terrain-tiles': {
            type: 'raster-dem',
            tiles: ['https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png'],
            encoding: 'terrarium',
            tileSize: 256,
            maxzoom: 14
          }
        },
        layers: [{
          id: 'satellite-layer',
          type: 'raster',
          source: 'esri-satellite',
          minzoom: 0, maxzoom: 19
        }],
        terrain: { source: 'terrain-tiles', exaggeration: 1.8 }
      },
      center: centerCoords,
      zoom: 13.5,
      pitch: 62,
      bearing: 45
    });
    
    new maplibregl.Marker({element: beaconEl})
      .setLngLat(centerCoords)
      .addTo(map);
      
    map.on('load', function() {
      function rotateCamera(timestamp) {
        map.rotateTo((timestamp / 500) % 360, { duration: 0 });
        requestAnimationFrame(rotateCamera);
      }
      rotateCamera(0);
    });

  } else if (window.L) {
    console.warn('⚠ WebGL not available or MapLibre failed. Falling back to Leaflet 2D Map.');
    document.getElementById('err-banner').style.display='block';
    document.getElementById('err-banner').innerText = '⚠ WEBGL UNAVAILABLE — FALLBACK TO 2D SATELLITE';
    
    var map = L.map('map', {zoomControl: false}).setView([centerCoords[1], centerCoords[0]], 15);
    L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {
      maxZoom: 19
    }).addTo(map);

    var customIcon = L.divIcon({
      className: 'custom-leaflet-marker',
      html: beaconEl.outerHTML,
      iconSize: [24, 24],
      iconAnchor: [12, 12]
    });
    
    var marker = L.marker([centerCoords[1], centerCoords[0]], {icon: customIcon}).addTo(map);
    setTimeout(function() {
      window._beaconEl = document.querySelector('.beacon-container');
    }, 500);
  }

  function connectSocket(){
    if(typeof io==='undefined') return;
    var sk=io();
    sk.on('connect',    function(){log('SOCKET_CONNECTED','ok');});
    sk.on('disconnect', function(){log('SOCKET_DISCONNECTED','w');});
    sk.on('new_lora_packet',function(data){
      if(data.status){log(data.status,'ok');return;}
      if(data.type==='alert'){
        var r=parseInt(data.rssi)||(-80); pktCommon(r);
        var pl=(data.payload||'').toUpperCase();
        if(pl.indexOf('SOS')>=0){
          setAlert(true,'SOS ACTIVE');log('⚠ '+pl,'d');
        } else if(pl.indexOf('SEISMIC')>=0||pl.indexOf('AI_')>=0||pl.indexOf('ELEPHANT')>=0){
          setAlert(true,'TARGET DETECTED');log('⚡ '+pl,'d');
        } else {
          log('📶 '+pl,'w');
        }
        log('  RSSI: '+r+' dBm','w');
        clearTimeout(_atimer); _atimer=setTimeout(function(){setAlert(false);},9000);
      }
    });
  }
  setTimeout(connectSocket,1200);
}
</script>"""
    
    new_content = content[:start_idx] + new_script + content[end_idx:]
    with open('d:\\SLIOT\\final\\Final\\templates\\dashboard.html', 'w', encoding='utf-8') as f:
        f.write(new_content)
    print('SUCCESS')
else:
    print('FAILED TO FIND MARKERS')
