#include <QImage>
#include <QDebug>
#include <QColor>
#include <iostream>
#include <iomanip>
#include <QFile>

using namespace std;

typedef struct {
    uchar blue;
    uchar green;
    uchar red;
    uchar alpha;
} color_t;

const color_t *pix_00;
int width;
int height;
int bytesOfColumn = 300;
int bytesOfRow = 64;
int charX = 6;
int charY = 14;
int startX;
int startY;
char *baseAscii;
uint32_t *baseMap;
int *indexTable;
int baseLen;

void test_print(int xStart, int yStart, int xMax, int yMax)
{
    int x, y;
    const color_t *pix;

    cout << "First Print:" << endl;
    cout << "    ";

    for(x = xStart; x < xMax; x++)
    {
        if(x % 0x04)
        {
            cout << "+";
        }
        else
        {
            cout << (x / 0x04);
        }
    }

    cout << endl;

    for(y = height-1-yMax; y < height-yStart; y++)
    {
        pix = y * width + pix_00 + xStart;
        cout << setw(4) << y;

        for(x = xStart; x < xMax; x++)
        {
            if(pix->blue > 0x7F)
            {
                cout << '.';
            }
            else
            {
                cout << '#';
            }

            pix++;
        }

        cout << endl;
    }
}

void print_char(int row, int column, int num)
{
    int x, y;
    int xTmp, yTmp;
    int xMax, yMax;
    const color_t *pix;

    xTmp = column * charX + startX;
    xMax = xTmp + charX * num;
    yTmp = row * charY + startY;
    yMax = yTmp + charY;

    for(y = yTmp; y < yMax; y++)
    {
        pix = pix_00 + y * width + xTmp;

        for(x = xTmp; x < xMax; x++)
        {
            if(pix->blue > 0x7F)
            {
                cout << '.';
            }
            else
            {
                cout << '#';
            }

            pix++;
        }

        cout << endl;
    }
}

void get_char(int row, int column, uint32_t *buf)
{
    int x, y;
    int xTmp, yTmp;
    int xMax, yMax;
    const color_t *pix;
    const color_t *pixTmp;

    xTmp = column * charX + startX;
    xMax = charX;
    yTmp = row * charY + startY;
    yMax = charY;

    for(x = 0; x < charX; x++)
    {
        buf[x] = 0;
    }

    pixTmp = pix_00 + xTmp + yTmp * width;

    for(y = 0; y < yMax; y++)
    {
        pix = pixTmp;

        for(x = 0; x < xMax; x++)
        {
            buf[indexTable[x]] <<= 1;

            if(pix->blue < 0x7F)
            { // Block pix.
                buf[indexTable[x]] |= 1;
            }

            pix++;
        }

        pixTmp += width;
    }
}

void get_char_print(const uint32_t *buf)
{
    int i, j;
    int tmp;
    uint32_t *bufTmp1;

    bufTmp1 = new uint32_t [charX];
    memcpy(bufTmp1, buf, sizeof(uint32_t)*charX);

    tmp = 1U << (charY - 1);

    for(j = 0; j < charY; j++)
    {
        for(i = 0; i < charX; i++)
        {
            if(bufTmp1[indexTable[i]] & tmp)
            {
                cout << '#';
            }
            else
            {
                cout << '.';
            }

            bufTmp1[indexTable[i]] <<= 1;
        }

        cout << endl;
    }

    delete [] bufTmp1;
}

int save_base_file(const char *fileName, const char *baseArray, int baseLen)
{
    QFile file(fileName);
    int tmp[3] = {baseLen, charX, charY};
    int i;
    uint32_t *base;

    if(!file.open(QFile::WriteOnly))
    {
        cout << "The file create error." << endl;

        return 1;
    }

    file.write((const char *)tmp, sizeof(tmp));
    file.write(baseArray, baseLen);
    base = new uint32_t [charX];

    for(i = 0; i < baseLen; i++)
    {
        get_char(0, i, base);
        file.write((const char *)base, charX*sizeof(uint32_t));
        get_char_print(base);
    }

    delete [] base;
    file.flush();
    file.close();

    return 0;
}

int restore_base_file(const char *fileName)
{
    QFile file(fileName);

    if(!file.open(QFile::ReadOnly))
    {
        cout << "The file open error." << endl;

        return 1;
    }

    file.read((char *)&baseLen, sizeof(baseLen));
    file.read((char *)&charX, sizeof(charX));
    file.read((char *)&charY, sizeof(charY));
    baseAscii = new char [baseLen];
    baseMap = new uint32_t [charX*baseLen];
    file.read((char *)baseAscii, baseLen);
    file.read((char *)baseMap, charX*baseLen*4);
    file.close();

    return 0;
}

int find_pos(void)
{
    const color_t *pix;
    int m, i, j;
    int count;

    for(m = height-1; m > charY; m--)
    {
        // Left lower corner and offset a char '/'.
        pix = m * width + pix_00 + charX;
        count = 0;

        for(i = 0; i < width; i++)
        {
            if(pix->blue < 0x7F)
            { // Block pix.
                count++;
            }
            else
            {
                if(count == charX)
                { // Match char width: "AZ-MB".
                    // Goto 'B' of "YB".
                    pix -= width * (charY - 3);

                    if(pix->blue > 0x7F)
                    { // White pix.
                        pix--;

                        // Match char width: "Y-X".
                        for(j = 0; j < charX; j++)
                        {
                            if(pix->blue > 0x7F)
                            { // White pix.
                                goto NEXT;
                            }

                            pix--;
                        }

                        // At 'A' of "AX".
                        if(pix->blue > 0x7F)
                        { // White pix, match "AZ-MB" "BYXA".

                            // TODO: match others.

                            // Realy pos.
                            startX = (pix - pix_00) % width;
                            startY = (pix - pix_00) / width;
                            // Offset pos.
                            startX -= charX - 1;
                            startY -= charY * (bytesOfRow - 1) + 1;

                            if((startX < 0) || (startY < 0))
                            {
                                cout << "This image is not complete." << endl;
                                cout << "Pos at: " << startX << "," << startY << endl;

                                return 1;
                            }

                            cout << "Find Succeed: " << startX << "," << startY << endl;

                            return 0;
                        }
                    }
                }

NEXT:
                // Not match char width.
                count = 0;
            }

            pix++;
        }
    }

    cout << "Not find.\n";

    return 1;
}

void get_data_mode(void)
{
    int *p;
    int i;

    indexTable = new int [charX];
    p = indexTable;

    if(charX % 2)
    {
        i = charX - 1;
    }
    else
    {
        i = charX - 2;
    }

    for(; i != -2; i -= 2)
    {
        *p++ = i;
    }

    for(i = 1; i < charX; i+= 2);
    {
        *p++ = i;
    }
}

int compare_base_map(const uint32_t *buf)
{
    const uint32_t *mapTmp = baseMap;
    const uint32_t *bufTmp = buf;
    int i, j;

    for(i = 0; i < baseLen; i++)
    {
        for(j = 0; j < charX; j++)
        {
            if(mapTmp[j] != bufTmp[j])
            {
                goto NEXT;
            }
        }

        break;

NEXT:
        mapTmp += charX;
    }

    if(i == baseLen)
    {
        //cout << "Scan error." << endl;
        return -1;
    }

    return baseAscii[i];
}

// Image recognition.
// argv[1]: file name "*.png"

#define BASE_MODE   0

// BaseImage recognition.

//#define BASE_MODE   1

// Turn hex to bin

#define HEX_BIN 1


int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

#if HEX_BIN == 1
    QFile fileHex(argv[1]);
    QFile fileBin(QString(argv[1]).replace(".hex", ".bin"));
    QByteArray ba;

    fileHex.open(QFile::ReadOnly);
    fileBin.open(QFile::WriteOnly);

    ba.clear();

    do {
        fileBin.write(ba);
        ba = QByteArray::fromHex(fileHex.readLine());
    } while(!ba.isEmpty());

    fileHex.close();
    fileBin.flush();
    fileBin.close();

    return 0;
#endif

#if HEX_BIN == 0
    QImage baseImage;

#if BASE_MODE == 0
    int i, j;
    int ret;
    uint32_t *buf;
    QString fileName(argv[1]);
    QString imageName(argv[1]);
    QFile file(fileName.replace(".PNG", ".hex"));

    baseImage.load(imageName);
#else
    baseImage.load("1.png");
#endif

    width = baseImage.width();
    height = baseImage.height();
    cout << "Width: " << width << endl;
    cout << "Height: " << height << endl;
    pix_00 = (const color_t *)baseImage.constBits();
    get_data_mode();

#if BASE_MODE == 1
    test_print(0, 0, 40, 40);
#endif

    if(find_pos())
    {
        delete [] indexTable;

        return 1;
    }

#if BASE_MODE == 1
    test_print(0, height-40, 40, height-1);
    save_base_file("1.bin", "0123456789ABCDEF", 16);
#else
    restore_base_file("1.bin");
    buf = new uint32_t [charX];
    file.open(QFile::WriteOnly);

    for(i = 0; i < bytesOfRow-1; i++)
    {
        for(j = 0; j < bytesOfColumn; j++)
        {
            get_char(i, j, buf);
            ret = compare_base_map(buf);

            if(ret == -1)
            {
                ret = '\n';
                /* NOTE: ret is a int type, here write 1 byte,
                   because target CPU is little endian. */
                file.write((const char *)&ret, 1);
                file.flush();
                file.close();

                return 0;
            }

            file.write((const char *)&ret, 1);
        }

        ret = '\n';
        file.write((const char *)&ret, 1);
    }

    file.flush();
    file.close();

    // TODO: can't delete.
    //delete [] buf;
    //delete [] indexTable;
    return 0;
#endif
#endif
}
