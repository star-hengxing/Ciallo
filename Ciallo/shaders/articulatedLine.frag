#version 460

layout(location = 0) in vec4 fragColor;
layout(location = 1) in flat vec2 p0;
layout(location = 2) in flat vec2 p1;
layout(location = 3) in vec2 p;
layout(location = 4) in float halfThickness;
layout(location = 5) in flat float summedLength;
layout(location = 6) in flat float hthickness[2]; // only being used by perfect vanilla

layout(location = 0) out vec4 outColor;

// #define STAMP
// #define AIRBRUSH

#ifdef STAMP
layout(location = 2) uniform float uniThickness;
layout(location = 3, binding = 0) uniform sampler2D stamp;
layout(location = 4) uniform float stampIntervalRatio;
layout(location = 5) uniform float noiseFactor;
layout(location = 6) uniform float rotationRand;
#endif

#ifdef AIRBRUSH
layout(location = 3, binding = 0) uniform sampler1D gradientSampler;
float sampleGraident(float distance){ return texture(gradientSampler, distance).r; }
#endif

mat2 rotate(float angle){
    return mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
}

// ---------------- Noise helper functions, ignore them ------------------
float random (in vec2 st) {
    return fract(sin(dot(st.xy,
                         vec2(12.9898,78.233)))*
        43758.5453123);
}

float noise (in vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}

#define OCTAVES 6
float fbm (in vec2 st) {
    // Initial values
    float value = 0.0;
    float amplitude = .5;
    float frequency = 0.;
    //
    // Loop of octaves
    for (int i = 0; i < OCTAVES; i++) {
        value += amplitude * noise(st);
        st *= 2.;
        amplitude *= .5;
    }
    return value;
}
// ---------------- Noise end ------------------

void main() {
    
    vec2 lHat = normalize(p1 - p0);
    vec2 hHat = vec2(-lHat.y, lHat.x);
    float len = length(p1-p0);

    // In LH coordinate
    vec2 pLH = vec2(dot(p-p0, lHat), dot(p-p0, hHat));
    vec2 p0LH = vec2(0, 0);
    vec2 p1LH = vec2(len, 0);

    float d0 = distance(p, p0);
    float d1 = distance(p, p1);

#if !defined(STAMP) && !defined(AIRBRUSH)
    // - Naive vanilla (OK if stroke is opaque)
    if(pLH.x < 0 && d0 > halfThickness){
        discard;
    }
    if(pLH.x > p1LH.x && d1 > halfThickness){
        discard;
    }
    outColor = fragColor;
    return;

    // - Transparent vanilla (perfectly handle transparency and self overlapping)
    //  use uninterpolated(flat) thickness avoid the joint mismatch.
    // if(pLH.x < 0 && d0 > hthickness[0]){
    //     discard;
    // }
    // if(pLH.x > p1LH.x && d1 > hthickness[1]){
    //     discard;
    // }
    // if(d0 < hthickness[0] && d1 < hthickness[1]){
    //     discard;
    // }
    // float A = fragColor.a;
    // if(d0 < hthickness[0] || d1 < hthickness[1]){
    //     A = 1.0 - sqrt(1.0 - fragColor.a);
    // }
    // outColor = vec4(fragColor.rgb, A);
    // return;
#endif

#ifdef STAMP
    float stampInterval = stampIntervalRatio * uniThickness;

    // first stamp and its index can be reached by this pixel.
    float stampStarting, stampStartingIndex;
    float frontEdge = pLH.x-halfThickness;
    if(frontEdge <= 0){
        stampStarting = mod(stampInterval - mod(summedLength, stampInterval), stampInterval); // mod twice for getting zero value
        stampStartingIndex = ceil(summedLength/stampInterval);
    }
    else{
        stampStarting = mod(stampInterval - mod(summedLength+frontEdge, stampInterval), stampInterval) + frontEdge;
        stampStartingIndex = ceil((summedLength+frontEdge)/stampInterval);
    }
    float backEdge = pLH.x+halfThickness;
    float stampEnding = (backEdge < len) ? backEdge:len;
    if(stampStarting > stampEnding) discard; // There are no stamps in this rect.

    float currStamp = stampStarting, currStampIndex = stampStartingIndex;
    float A = 0;
    int san_i = 0, MAX_i = 32; // sanity check to avoid infinite loop
    do{
        san_i += 1;
        if(san_i > MAX_i) break;
        // Sample on stamp and manually blend alpha
        float angle = rotationRand*radians(360*fract(sin(currStampIndex)*1.0));
        vec2 vStamp = pLH - vec2(currStamp, 0);
        vStamp *= rotate(angle);
        vec2 uv = (vStamp/halfThickness + 1.0)/2.0;

        vec4 color = texture(stamp, uv);
        float alpha = clamp(color.a - noiseFactor*fbm(uv*50.0), 0.0, 1.0);
        A = A * (1.0-alpha) + alpha;
        currStamp += stampInterval; 
        currStampIndex += 1.0;
    }while(currStamp < stampEnding);
    if(A < 1e-4){
        discard;
    }
    outColor = vec4(fragColor.rgb, A*fragColor.a);
    return;
#endif

#ifdef AIRBRUSH
    float A = fragColor.a;

    if((pLH.x < 0 && d0 > halfThickness)){
        discard;
    }
    if((pLH.x > p1LH.x && d1 > halfThickness)){
        discard;
    }

    // normalize
    pLH = pLH / halfThickness;
    d0 /= halfThickness;
    d1 /= halfThickness;
    len /= halfThickness;

    float reversedGradBone = 1.0-A*sampleGraident(pLH.y);

    float exceed0, exceed1;
    exceed0 = exceed1 = 1.0;
    
    if(d0 < 1.0) {
      exceed0 = pow(1.0-A*sampleGraident(d0), 
        sign(pLH.x) * 1.0/2.0 * (1.0-abs(pLH.x))) * 
        pow(reversedGradBone, step(0.0, -pLH.x));
    }
    if(d1 < 1.0) {
      exceed1 = pow(1.0-A*sampleGraident(d1), 
        sign(len - pLH.x) * 1.0/2.0 * (1.0-abs(len-pLH.x))) * 
        pow(reversedGradBone, step(0.0, pLH.x - len));
    }
    A = clamp(1.0 - reversedGradBone/exceed0/exceed1, 0.0, 1.0-1e-3);
    outColor = vec4(fragColor.rgb, A);
    return;
#endif
    
}
