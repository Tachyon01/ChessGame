/*
Author: Pulkit Goel
Class: ECE6122 (section)
Last Date Modified: 12/02/2024
Description:
Structures for storing various pieces parameters
*/

//will use only 95 to 104
float files[105];
float killFile[32];

//x axis
//took 0 as spare
float ranks[9];
float killRank[32];




struct Camera
{
    float radius = 50.0f;
    float horizontalAngle = glm::radians(0.0f);
    float verticalAngle = glm::radians(20.0f);

    float x = 17.1;
    float y = 47.0;
    float z = 0.0;


    void get_Cartesian()
    {
        x = radius * sin(verticalAngle) * cos(horizontalAngle);
        z = radius * cos(verticalAngle);
        y = radius * sin(verticalAngle) * sin(horizontalAngle);
    }

    //bool specularDiffuseEnabled = true;
};


struct Light
{
    float radius = 25.0f;
    float horizontalAngle = glm::radians(25.0f);
    float verticalAngle = glm::radians(25.0f);

    //Values here are cartesian
    float x = 25.0;
    float y = 25.0;
    float z = 25.0;

    void get_Cartesian()
    {
        x = radius * sin(verticalAngle) * cos(horizontalAngle);
        z = radius * cos(verticalAngle);
        y = radius * sin(verticalAngle) * sin(horizontalAngle);
    }

    float power = 3000.0f;
    //bool specularDiffuseEnabled = true;
};

//Components for 1 piece
struct Piece
{
    glm::vec3 name;
    float xPos = 0.0f;
    float yPos = 0.0f;
    float zPos = 0.0f;

    char file = 'a';
    int rank = 1;

    glm::vec3 pos = glm::vec3(xPos, yPos, zPos);

    bool isWhite = true;

    string variety = "Pawn";
};

//Struct for all chess pieces
struct AllPieces
{
    //Black pieces
    Piece blackPawn1;
    Piece blackPawn2;
    Piece blackPawn3;
    Piece blackPawn4;
    Piece blackPawn5;
    Piece blackPawn6;
    Piece blackPawn7;
    Piece blackPawn8;

    Piece blackRook1; //Elephant
    Piece blackKnight1; //Horse
    Piece blackBishop1; //Camel
    Piece blackQueen;
    Piece blackKing;
    Piece blackBishop2;
    Piece blackKnight2;
    Piece blackRook2;

    //white pieces
    Piece whitePawn1;
    Piece whitePawn2;
    Piece whitePawn3;
    Piece whitePawn4;
    Piece whitePawn5;
    Piece whitePawn6;
    Piece whitePawn7;
    Piece whitePawn8;

    Piece whiteRook1; //Elephant
    Piece whiteKnight1; //Horse
    Piece whiteBishop1; //Camel
    Piece whiteQueen;
    Piece whiteKing;
    Piece whiteBishop2;
    Piece whiteKnight2;
    Piece whiteRook2;

    void updatePos(Piece& p, char file, int rank)
    {
        p.file = file;
        p.rank = rank;

        p.zPos = 0.0f;
        p.xPos = ranks[rank];
        p.yPos = files[file];

        p.pos = glm::vec3(p.xPos, p.yPos, p.zPos);

        //std::cout << "Piece at r " << ranks[rank] << " file " << file <<" "<< files[file] << "\n";
    }
    void updatePos_kill(Piece& p, int killID)
    {
        p.file = killID;
        p.rank = killID;

        p.zPos = 0.0f;
        p.xPos = killRank[killID];
        p.yPos = killFile[killID];

        p.pos = glm::vec3(p.xPos, p.yPos, p.zPos);

        //std::cout << "Piece at r " << ranks[rank] << " file " << file <<" "<< files[file] << "\n";
    }
    void updatePos_animate(Piece& p, float x, float y)
    {
        //p.zPos = 0.0f;
        p.xPos = x;
        p.yPos = y;
        p.pos = glm::vec3(p.xPos, p.yPos, p.zPos);

        //cout << " Positions " << p.xPos << " " << p.yPos << " " << p.zPos << endl;

    }

    void move_z(Piece& p, float i)
    {
        p.zPos += i;
        //p.xPos = x;
        //p.yPos = y;
        p.pos = glm::vec3(p.xPos, p.yPos, p.zPos);

        //cout << " Positions " << p.xPos << " " << p.yPos << " " << p.zPos << endl;

    }

};

//Hold obj models
struct Models
{
    CAssimpModel pawnModel_dark;
    CAssimpModel rookModel_dark;
    CAssimpModel knightModel_dark;
    CAssimpModel bishopModel_dark;
    CAssimpModel queenModel_dark;
    CAssimpModel kingModel_dark;
    CAssimpModel pawnModel_light;
    CAssimpModel rookModel_light;
    CAssimpModel knightModel_light;
    CAssimpModel bishopModel_light;
    CAssimpModel queenModel_light;
    CAssimpModel kingModel_light;
};