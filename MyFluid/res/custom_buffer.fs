#version 430 core
out vec4 FragColor;

in vec2 TexCoords;


uniform sampler2D density;
uniform sampler2D obstacle;
uniform sampler2D divergence;
uniform sampler2D pressure;
uniform sampler2D velocity;


uniform vec3 viewPos;

void main()
{             
   
 
    vec3 den = texture(density,TexCoords).rgb/10.0;
    vec3 o = texture(obstacle,TexCoords).rgb;
    vec3 div = texture(divergence,TexCoords).rgb;
    vec3 p = texture(pressure,TexCoords).rgb;
    vec3 v = texture(velocity,TexCoords).rgb;
    
    vec4 col = vec4(den.x* vec3(0.1, 1,1), 1);
    float speed = length(v);
    col += vec4(vec3(0.4,0.0,1)*div.x, 0.0);

    if(o.x == 0.0){
        
        col = vec4(0.9,0.0,0.1,1);

	}

    col = vec4(col.rgb/1.50, 1.0);
    
    FragColor = col;
}
