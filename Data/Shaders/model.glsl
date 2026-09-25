#type vertex
#version 410 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;
layout(location = 5) in ivec4 a_BoneIds; 
layout(location = 6) in vec4 a_Weights;
layout(location = 7) in int a_EntityID;

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
} vs_out;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

uniform vec3 u_LightPos;
uniform vec3 u_ViewPos;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 u_FinalBonesMatrices[MAX_BONES];

layout (location = 4) flat out int v_EntityID;
uniform int u_EntityID;

void main()
{    
    vs_out.TexCoords = a_TexCoord;    
    v_EntityID = u_EntityID;

    mat4 skin = mat4(0.0);
    float totalWeight = 0.0;
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        if (a_BoneIds[i] < 0 || a_BoneIds[i] >= MAX_BONES || a_Weights[i] <= 0.0) continue;
        skin += u_FinalBonesMatrices[a_BoneIds[i]] * a_Weights[i];
        totalWeight += a_Weights[i];
    }
    skin = totalWeight > 0.0 ? skin / totalWeight : mat4(1.0);
    vec4 skinnedPosition = skin * vec4(a_Position, 1.0);
    vec3 skinnedNormal = normalize(mat3(skin) * a_Normal);
    vec3 skinnedTangent = mat3(skin) * a_Tangent;

    vec4 worldPos = u_Model * skinnedPosition;

    vs_out.FragPos = worldPos.xyz;
    
    mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
    vec3 N = normalize(normalMatrix * skinnedNormal);
    vec3 T = normalMatrix * skinnedTangent;
    T -= dot(T, N) * N;
    // Untextured meshes may have no UVs/tangents. Build a valid basis so
    // their lighting does not collapse to ambient-only or produce NaNs.
    if (dot(T, T) < 0.000001)
        T = cross(abs(N.y) < 0.99 ? vec3(0,1,0) : vec3(1,0,0), N);
    T = normalize(T);
    vec3 B = cross(N, T);
    
    mat3 TBN = transpose(mat3(T, B, N));    
    vs_out.TangentLightPos = TBN * u_LightPos;
    vs_out.TangentViewPos  = TBN * u_ViewPos;
    vs_out.TangentFragPos  = TBN * vs_out.FragPos;
            
    gl_Position = u_Projection * u_View * worldPos;
}

#type fragment
#version 410 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out int color2;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
} fs_in;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_normal1;

uniform vec3 u_LightPos;
uniform vec3 u_ViewPos;
uniform vec3 u_Color;
uniform vec3 u_LightColor;
uniform float u_Transparancy;

uniform bool u_UseNormalMap;
uniform bool u_UseDiffuseMap;

layout (location = 4) flat in int v_EntityID;

void main()
{
     // obtain normal from normal map in range [0,1]
    vec3 normal;
    if (u_UseNormalMap)
    {
        normal = texture(texture_normal1, fs_in.TexCoords).rgb;
        normal = normalize(normal * 2.0 - 1.0);
    }
    else
    {
        normal = vec3(0.0, 0.0, 1.0); // Tangent space default normal (straight up)
    }            
   
   vec3 color;
    // get diffuse color
    if(u_UseDiffuseMap)
    {
        color = texture(texture_diffuse1, fs_in.TexCoords).rgb;
        if(color == vec3(0))
            color = u_Color;

    }
    else{
        color = u_Color;
    }
    // ambient
    vec3 ambient = 0.18 * color;
    // diffuse
    vec3 lightDir = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * color;
    // specular
    vec3 viewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    vec3 specular = vec3(0.2) * spec;
    
    FragColor = vec4((ambient + diffuse + specular) * u_LightColor, u_Transparancy);
    
    color2 = v_EntityID;
}