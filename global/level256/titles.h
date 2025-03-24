#include <3ds/types.h>

bool isOnlineSupportedTitle(u64 titleId)
{
    switch (titleId)
    {
    case 0x000400000016C700ULL: // 赤猫団
    case 0x000400000016C600ULL: // 白犬隊
    case 0x00040000001CF000ULL: // Red cats (EUR)
    case 0x00040000001CEC00ULL: // White dogs (EUR)
    case 0x0004000000191000ULL: // スシ
    case 0x0004000000191100ULL: // テンプラ
    case 0x00040000001AF400ULL: // スキヤキ
    case 0x000400000012F900ULL: // 元祖
    case 0x000400000012F800ULL: // 本家
    case 0x0004000000155100ULL: // 真打
    // case 0x00040000001ADD00ULL: // 桃鉄
    case 0x00040000001B2900ULL: // 真打(EUR)
    case 0x000400000019AB00ULL: // 元祖(EUR)
    case 0x000400000019AC00ULL: // 本家(EUR)
    case 0x000400000019AA00ULL: // 妖怪ウォッチ2 (USA)
    case 0x000400000019A900ULL: // 妖怪ウォッチ2 (USA)
    case 0x00040000001B2700ULL: // 妖怪ウォッチ2 真打 (USA)
    case 0x00040000001D6800ULL: // 妖怪ウォッチ3 (EUR)
    case 0x00040000001D6900ULL: // 妖怪ウォッチ3 France
    case 0x00040000001D6A00ULL: // 妖怪ウォッチ3 German
    case 0x00040000001D6B00ULL: // 妖怪ウォッチ3 Italy
    case 0x00040000001D6C00ULL: // 妖怪ウォッチ3 Spain
    case 0x00040000001D6700ULL: // 妖怪ウォッチ3 (USA)
    case 0x00040000001CEB00ULL: // 妖怪ウォッチバスターズ(USA)
    case 0x00040000001CEF00ULL: // 妖怪ウォッチバスターズ(USA)
    case 0x000400000019B000ULL: // 妖怪ウォッチ2(オーストラリア)
    case 0x000400000019B100ULL: // 妖怪ウォッチ2(オーストラリア)
    case 0x00040000001B2800ULL: // 妖怪ウォッチ2真打(オーストラリア)
    case 0x000400000019AE00ULL: // 妖怪ウォッチ2 (Kor)
    case 0x000400000019AF00ULL: // 妖怪ウォッチ2 (Kor)
    case 0x00040000001B2A00ULL: // 妖怪ウォッチ2真打 (Kor)
    case 0x00040000001CF200ULL: // 妖怪ウォッチバスターズ (Kor)
    case 0x00040000001CED00ULL: // 妖怪ウォッチバスターズ (Kor)
    case 0x00040000000CF400ULL: // 妖怪ウォッチ1
    case 0x0004000000167800ULL: // 妖怪ウォッチ1 (EUR)
    case 0x0004000000167700ULL: // 妖怪ウォッチ1 (USA)
    case 0x000400000017C200ULL: // 妖怪ウォッチ1 (Au)
    case 0x0004000000167600ULL: // 妖怪ウォッチ1 (Kor)
    case 0x0004000000086200ULL: // とび森
    case 0x0004000000198D00ULL: // とび森 Amiibo+
    case 0x0004000000086400ULL: // とび森(EUR)
    case 0x0004000000198F00ULL: // とび森(EUR) Amiibo+
    case 0x0004000000086300ULL: // とび森(USA)
    case 0x0004000000198E00ULL: // とび森(USA) Amiibo+
    case 0x0004000000086500ULL: // とび森(KOR)
    case 0x0004000000199000ULL: // とび森(KOR) Amiibo+
    case 0x00040000001C9400ULL: // 妖怪ウォッチバスターズ2
    case 0x00040000001C9C00ULL: // 妖怪ウォッチバスターズ2
    case 0x000400000018B000ULL: // 妖怪三国志
    case 0x0004000000197100ULL: // MHXX(JP)
    //case 0x00040000001B8100ULL: // MHXX(TW)
        return true;
    default:
        break;
    }

    return false;
}

bool isPatchProhibitedTitle(u64 titleId)
{
    switch (titleId)
    {
    case 0x000400000016C700ULL: // 赤猫団
    case 0x000400000016C600ULL: // 白犬隊
    case 0x00040000001CF000ULL: // Red cats (EUR)
    case 0x00040000001CEC00ULL: // White dogs (EUR)
    case 0x0004000000191000ULL: // スシ
    case 0x0004000000191100ULL: // テンプラ
    case 0x00040000001AF400ULL: // スキヤキ
    case 0x000400000012F900ULL: // 元祖
    case 0x000400000012F800ULL: // 本家
    case 0x0004000000155100ULL: // 真打
    case 0x00040000001ADD00ULL: // 桃鉄
    case 0x00040000001B2900ULL: // 真打(EUR)
    case 0x000400000019AB00ULL: // 元祖(EUR)
    case 0x000400000019AC00ULL: // 本家(EUR)
    case 0x000400000019AA00ULL: // 妖怪ウォッチ2 (USA)
    case 0x000400000019A900ULL: // 妖怪ウォッチ2 (USA)
    case 0x00040000001B2700ULL: // 妖怪ウォッチ2 真打 (USA)
    case 0x00040000001D6800ULL: // 妖怪ウォッチ3 (EUR)
    case 0x00040000001D6900ULL: // 妖怪ウォッチ3 France
    case 0x00040000001D6A00ULL: // 妖怪ウォッチ3 German
    case 0x00040000001D6B00ULL: // 妖怪ウォッチ3 Italy
    case 0x00040000001D6C00ULL: // 妖怪ウォッチ3 Spain
    case 0x00040000001D6700ULL: // 妖怪ウォッチ3 (USA)
    case 0x00040000001CEB00ULL: // 妖怪ウォッチバスターズ(USA)
    case 0x00040000001CEF00ULL: // 妖怪ウォッチバスターズ(USA)
    case 0x000400000019B000ULL: // 妖怪ウォッチ2(オーストラリア)
    case 0x000400000019B100ULL: // 妖怪ウォッチ2(オーストラリア)
    case 0x00040000001B2800ULL: // 妖怪ウォッチ2真打(オーストラリア)
    case 0x000400000019AE00ULL: // 妖怪ウォッチ2 (Kor)
    case 0x000400000019AF00ULL: // 妖怪ウォッチ2 (Kor)
    case 0x00040000001B2A00ULL: // 妖怪ウォッチ2真打 (Kor)
    case 0x00040000001CF200ULL: // 妖怪ウォッチバスターズ (Kor)
    case 0x00040000001CED00ULL: // 妖怪ウォッチバスターズ (Kor)
    case 0x00040000000CF400ULL: // 妖怪ウォッチ1
    case 0x0004000000167800ULL: // 妖怪ウォッチ1 (EUR)
    case 0x0004000000167700ULL: // 妖怪ウォッチ1 (USA)
    case 0x000400000017C200ULL: // 妖怪ウォッチ1 (Au)
    case 0x0004000000167600ULL: // 妖怪ウォッチ1 (Kor)
    case 0x000400000018B000ULL: // 妖怪三国志
    case 0x0004000000197100ULL: // MHXX(JP)
    //case 0x00040000001B8100ULL: // MHXX(TW)
                                /*case 0x0004000000086200ULL: // とび森
                                case 0x0004000000198D00ULL: // とび森 Amiibo+
                                case 0x0004000000086400ULL: // とび森(EUR)
                                case 0x0004000000198F00ULL: // とび森(EUR) Amiibo+
                                case 0x0004000000086300ULL: // とび森(USA)
                                case 0x0004000000198E00ULL: // とび森(USA) Amiibo+
                                case 0x0004000000086500ULL: // とび森(KOR)
                                case 0x0004000000199000ULL: // とび森(KOR) Amiibo+
                                                            //case 0x00040000001C9400ULL: // 妖怪ウォッチバスターズ2
                                                            //case 0x00040000001C9C00ULL: // 妖怪ウォッチバスターズ2*/
        return true;
    default:
        break;
    }

    return false;
}