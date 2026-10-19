#include "UiIcons.h"

#if defined(HAVE_QT6)
#include <QSvgRenderer>
#include <QPainter>
#include <QByteArray>
#include <QString>
#include <QGuiApplication>
#include <QPixmapCache>
#include <cmath>

namespace catchim::ui {

const char* UiIcons::getSvgContent(UiIcon icon) {
    switch (icon) {
        case UiIcon::Music:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="6.5" cy="18.5" r="3.5" stroke="%1" stroke-width="1.5"/><circle cx="18" cy="16" r="3" stroke="%1" stroke-width="1.5"/><path d="M10 18.5L10 7C10 6.07655 10 5.61483 10.2635 5.32794C10.5269 5.04106 11.0175 4.9992 11.9986 4.91549C16.022 4.57222 18.909 3.26005 20.3553 2.40978C20.6508 2.236 20.7986 2.14912 20.8993 2.20672C21 2.26432 21 2.4315 21 2.76587V16" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M10 10C15.8667 10 19.7778 7.66667 21 7" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/></svg>)";
        case UiIcon::Transform:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M8.00001 3.09779C8.00001 3.09779 4.03375 2.74194 3.38784 3.38785C2.74192 4.03375 3.09784 8 3.09784 8" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M8.00001 20.9022C8.00001 20.9022 4.03375 21.2581 3.38784 20.6122C2.74192 19.9662 3.09784 16 3.09784 16" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M16 3.09779C16 3.09779 19.9663 2.74194 20.6122 3.38785C21.2581 4.03375 20.9022 8 20.9022 8" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M16 20.9022C16 20.9022 19.9663 21.2581 20.6122 20.6122C21.2581 19.9662 20.9022 16 20.9022 16" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M14.0107 9.99847L20.0625 3.94678" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M9.99696 14.0024L3.63966 20.3807" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M9.99732 10.0024L3.84571 3.85889" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M13.9795 14.0024L20.5279 20.4983" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/></svg>)";
        case UiIcon::Font:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M14 19L11.1069 10.7479C9.76348 6.91597 9.09177 5 8 5C6.90823 5 6.23652 6.91597 4.89309 10.7479L2 19M4.5 12H11.5" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M21.9692 13.9392V18.4392M21.9692 13.9392C22.0164 13.1161 22.0182 12.4891 21.9194 11.9773C21.6864 10.7709 20.4258 10.0439 19.206 9.89599C18.0385 9.75447 17.1015 10.055 16.1535 11.4363M21.9692 13.9392L19.1256 13.9392C18.6887 13.9392 18.2481 13.9603 17.8272 14.0773C15.2545 14.7925 15.4431 18.4003 18.0233 18.845C18.3099 18.8944 18.6025 18.9156 18.8927 18.9026C19.5703 18.8724 20.1955 18.545 20.7321 18.1301C21.3605 17.644 21.9692 16.9655 21.9692 15.9392V13.9392Z" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/></svg>)";
        case UiIcon::Drop:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M3.5 13.678C3.5 9.49387 7.08079 5.35907 9.59413 2.97222C10.9591 1.67593 13.0409 1.67593 14.4059 2.97222C16.9192 5.35907 20.5 9.49387 20.5 13.678C20.5 17.7804 17.2812 22 12 22C6.71878 22 3.5 17.7804 3.5 13.678Z" stroke="%1" stroke-width="1.5"/></svg>)";
        case UiIcon::Speed:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="18" r="3" stroke="%1" stroke-width="1.5"/><path d="M12 15V10" stroke="%1" stroke-linecap="round" stroke-width="1.5"/><path d="M22 13C22 7.47715 17.5228 3 12 3C6.47715 3 2 7.47715 2 13" stroke="%1" stroke-linecap="round" stroke-width="1.5"/></svg>)";
        case UiIcon::CloudUpload:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M17.4776 9.01106C17.485 9.01102 17.4925 9.01101 17.5 9.01101C19.9853 9.01101 22 11.0294 22 13.5193C22 15.8398 20.25 17.7508 18 18M17.4776 9.01106C17.4924 8.84606 17.5 8.67896 17.5 8.51009C17.5 5.46695 15.0376 3 12 3C9.12324 3 6.76233 5.21267 6.52042 8.03192M17.4776 9.01106C17.3753 10.1476 16.9286 11.1846 16.2428 12.0165M6.52042 8.03192C3.98398 8.27373 2 10.4139 2 13.0183C2 15.4417 3.71776 17.4632 6 17.9273M6.52042 8.03192C6.67826 8.01687 6.83823 8.00917 7 8.00917C8.12582 8.00917 9.16474 8.38194 10.0005 9.01101"/><path d="M12 13V21M12 13C11.2998 13 9.99153 14.9943 9.5 15.5M12 13C12.7002 13 14.0085 14.9943 14.5 15.5"/></svg>)";
        case UiIcon::Media:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M2.36064 15.1788C1.98502 13.2956 1.79721 12.354 2.33084 11.7159C2.36642 11.6734 2.40405 11.6323 2.44361 11.5927C3.03686 11 4.08674 11 6.1865 11H17.8135C19.9133 11 20.9631 11 21.5564 11.5927C21.5959 11.6323 21.6336 11.6734 21.6692 11.7159C22.2028 12.354 22.015 13.2956 21.6394 15.1788C21.0993 17.8865 20.8292 19.2404 19.8109 20.0721C19.7414 20.1288 19.6698 20.1833 19.5961 20.2354C18.5163 21 17.0068 21 13.9876 21H10.0124C6.99323 21 5.48367 21 4.40387 20.2354C4.33022 20.1833 4.2586 20.1288 4.18914 20.0721C3.17075 19.2404 2.90072 17.8865 2.36064 15.1788Z" stroke="%1" stroke-width="1.5"/><path d="M4 11V5.5C4 4.11929 5.11929 3 6.5 3H8.92963C9.59834 3 10.2228 3.3342 10.5937 3.8906L12 6M12 6H8.5M12 6H17.5C18.8807 6 20 7.11929 20 8.5V11" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/></svg>)";

        case UiIcon::Audio:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M20.0849 17C20.5849 15.5 21 13.4368 21 12C21 7.02944 16.9706 3 12 3C7.02944 3 3 7.02944 3 12C3 13.4368 3.41512 15.5 3.91512 17"/><path d="M8.97651 19.6043L7.23857 14.6127C7.05341 14.1466 6.4617 13.9131 5.97493 14.0297C4.46441 14.5333 3.6462 16.1718 4.14742 17.6895L4.58543 19.0158C5.08664 20.5334 6.71747 21.3555 8.22799 20.8519C8.68896 20.6556 9.10449 20.0897 8.97651 19.6043Z"/><path d="M15.0235 19.6043L16.7614 14.6127C16.9466 14.1466 17.5383 13.9131 18.0251 14.0297C19.5356 14.5333 20.3538 16.1718 19.8526 17.6895L19.4146 19.0158C18.9134 20.5334 17.2825 21.3555 15.772 20.8519C15.311 20.6556 14.8955 20.0897 15.0235 19.6043Z"/></svg>)";

        case UiIcon::Text:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M15 21.001H9" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M12 3.00001V21.0008M12 3.00001C13.3874 3.00001 15.1695 3.03055 16.5884 3.17649C17.1885 3.2382 17.4886 3.26906 17.7541 3.37791C18.3066 3.60429 18.7518 4.10063 18.9194 4.67681C19 4.95382 19 5.26992 19 5.90215M12 3.00001C10.6126 3.00001 8.83047 3.03055 7.41161 3.17649C6.8115 3.2382 6.51144 3.26906 6.24586 3.37791C5.69344 3.60429 5.24816 4.10063 5.08057 4.67681C5 4.95382 5 5.26992 5 5.90215" stroke="%1" stroke-linecap="round" stroke-width="1.5"/></svg>)";

        case UiIcon::Stickers:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M16 9V9.01" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="2.5"/><path d="M8 9V9.01" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="2.5"/><path d="M8 14.5C8.91212 15.7144 10.3643 16.5 12 16.5C13.6357 16.5 15.0879 15.7144 16 14.5" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/></svg>)";

        case UiIcon::Effects:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M16.6917 9.80279L18.4834 8.01108C19.1722 7.32225 19.1722 6.20545 18.4834 5.51662C17.7946 4.82779 16.6777 4.82779 15.9889 5.51662L14.1972 7.30833M16.6917 9.80279L6.01108 20.4834C5.32225 21.1722 4.20545 21.1722 3.51662 20.4834C2.82779 19.7946 2.82779 18.6777 3.51662 17.9889L14.1972 7.30833M16.6917 9.80279L14.1972 7.30833" stroke="%1" stroke-linejoin="round" stroke-width="1.5"/><path d="M17.9737 14.0215C17.9795 13.9928 18.0205 13.9928 18.0263 14.0215C18.3302 15.5081 19.4919 16.6698 20.9785 16.9737C21.0072 16.9795 21.0072 17.0205 20.9785 17.0263C19.4919 17.3302 18.3302 18.4919 18.0263 19.9785C18.0205 20.0072 17.9795 20.0072 17.9737 19.9785C17.6698 18.4919 16.5081 17.3302 15.0215 17.0263C14.9928 17.0205 14.9928 16.9795 15.0215 16.9737C16.5081 16.6698 17.6698 15.5081 17.9737 14.0215Z" stroke="%1" stroke-linejoin="round" stroke-width="1.5"/><path d="M8.12063 3.30967C8.20503 2.89678 8.79497 2.89678 8.87937 3.30967C9.06576 4.22159 9.77841 4.93424 10.6903 5.12063C11.1032 5.20503 11.1032 5.79497 10.6903 5.87937C9.77841 6.06576 9.06576 6.77841 8.87937 7.69033C8.79497 8.10322 8.20503 8.10322 8.12063 7.69033C7.93424 6.77841 7.22159 6.06576 6.30967 5.87937C5.89678 5.79497 5.89678 5.20503 6.30967 5.12063C7.22159 4.93424 7.93424 4.22159 8.12063 3.30967Z" fill="%1"/></svg>)";

        case UiIcon::Transitions:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M12.5 18C12.5 18 18.5 13.5811 18.5 12C18.5 10.4188 12.5 6 12.5 6" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/><path d="M5.50005 18C5.50005 18 11.5 13.5811 11.5 12C11.5 10.4188 5.5 6 5.5 6" stroke="%1" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"/></svg>)";

        case UiIcon::Captions:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12C2 8.02033 2 6.03049 3.0528 4.70201C3.22119 4.48953 3.40678 4.29302 3.60746 4.11473C4.86213 3 6.74142 3 10.5 3H13.5C17.2586 3 19.1379 3 20.3925 4.11473C20.5932 4.29302 20.7788 4.48953 20.9472 4.70201C22 6.03049 22 8.02033 22 12C22 15.9797 22 17.9695 20.9472 19.298C20.7788 19.5105 20.5932 19.707 20.3925 19.8853C19.1379 21 17.2586 21 13.5 21H10.5C6.74142 21 4.86213 21 3.60746 19.8853C3.40678 19.707 3.22119 19.5105 3.0528 19.298C2 17.9695 2 15.9797 2 12Z" stroke="%1" stroke-linecap="round" stroke-width="1.5"/><path d="M10.5 9H10C9.06812 9 8.60218 9 8.23463 9.15224C7.74458 9.35523 7.35523 9.74458 7.15224 10.2346C7 10.6022 7 11.0681 7 12C7 12.9319 7 13.3978 7.15224 13.7654C7.35523 14.2554 7.74458 14.6448 8.23463 14.8478C8.60218 15 9.06812 15 10 15H10.5M17 9H16.5C15.5681 9 15.1022 9 14.7346 9.15224C14.2446 9.35523 13.8552 9.74458 13.6522 10.2346C13.5 10.6022 13.5 11.0681 13.5 12C13.5 12.9319 13.5 13.3978 13.6522 13.7654C13.8552 14.2554 14.2446 14.6448 14.7346 14.8478C15.1022 15 15.5681 15 16.5 15H17" stroke="%1" stroke-linecap="round" stroke-width="1.5"/></svg>)";

        case UiIcon::Settings:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M21.3175 7.14139L20.8239 6.28479C20.4506 5.63696 20.264 5.31305 19.9464 5.18388C19.6288 5.05472 19.2696 5.15664 18.5513 5.36048L17.3311 5.70418C16.8725 5.80994 16.3913 5.74994 15.9726 5.53479L15.6357 5.34042C15.2766 5.11043 15.0004 4.77133 14.8475 4.37274L14.5136 3.37536C14.294 2.71534 14.1842 2.38533 13.9228 2.19657C13.6615 2.00781 13.3143 2.00781 12.6199 2.00781H11.5051C10.8108 2.00781 10.4636 2.00781 10.2022 2.19657C9.94085 2.38533 9.83106 2.71534 9.61149 3.37536L9.27753 4.37274C9.12465 4.77133 8.84845 5.11043 8.48937 5.34042L8.15249 5.53479C7.73374 5.74994 7.25259 5.80994 6.79398 5.70418L5.57375 5.36048C4.85541 5.15664 4.49625 5.05472 4.17867 5.18388C3.86109 5.31305 3.67445 5.63696 3.30115 6.28479L2.80757 7.14139C2.45766 7.74864 2.2827 8.05227 2.31666 8.37549C2.35061 8.69871 2.58483 8.95918 3.05326 9.48012L4.0843 10.6328C4.3363 10.9518 4.51521 11.5078 4.51521 12.0077C4.51521 12.5078 4.33636 13.0636 4.08433 13.3827L3.05326 14.5354C2.58483 15.0564 2.35062 15.3168 2.31666 15.6401C2.2827 15.9633 2.45766 16.2669 2.80757 16.8741L3.30114 17.7307C3.67443 18.3785 3.86109 18.7025 4.17867 18.8316C4.49625 18.9608 4.85542 18.8589 5.57377 18.655L6.79394 18.3113C7.25263 18.2055 7.73387 18.2656 8.15267 18.4808L8.4895 18.6752C8.84851 18.9052 9.12464 19.2442 9.2775 19.6428L9.61149 20.6403C9.83106 21.3003 9.94085 21.6303 10.2022 21.8191C10.4636 22.0078 10.8108 22.0078 11.5051 22.0078H12.6199C13.3143 22.0078 13.6615 22.0078 13.9228 21.8191C14.1842 21.6303 14.294 21.3003 14.5136 20.6403L14.8476 19.6428C15.0004 19.2442 15.2765 18.9052 15.6356 18.6752L15.9724 18.4808C16.3912 18.2656 16.8724 18.2055 17.3311 18.3113L18.5513 18.655C19.2696 18.8589 19.6288 18.9608 19.9464 18.8316C20.264 18.7025 20.4506 18.3785 20.8239 17.7307L21.3175 16.8741C21.6674 16.2669 21.8423 15.9633 21.8084 15.6401C21.7744 15.3168 21.5402 15.0564 21.0718 14.5354L20.0407 13.3827C19.7887 13.0636 19.6098 12.5078 19.6098 12.0077C19.6098 11.5078 19.7888 10.9518 20.0407 10.6328L21.0718 9.48012C21.5402 8.95918 21.7744 8.69871 21.8084 8.37549C21.8423 8.05227 21.6674 7.74864 21.3175 7.14139Z" stroke="%1" stroke-linecap="round" stroke-width="1.5"/><path d="M15.5195 12C15.5195 13.933 13.9525 15.5 12.0195 15.5C10.0865 15.5 8.51953 13.933 8.51953 12C8.51953 10.067 10.0865 8.5 12.0195 8.5C13.9525 8.5 15.5195 10.067 15.5195 12Z" stroke="%1" stroke-width="1.5"/></svg>)";

        case UiIcon::Split:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M21 6C21 7.65685 19.6569 9 18 9C16.3431 9 15 7.65685 15 6C15 4.34315 16.3431 3 18 3C19.6569 3 21 4.34315 21 6Z"/><path d="M21 18C21 19.6569 19.6569 21 18 21C16.3431 21 15 19.6569 15 18C15 16.3431 16.3431 15 18 15C19.6569 15 21 16.3431 21 18Z"/><path d="M15 8L3 19"/><path d="M15.0003 16L11 12"/><path d="M3 5.00011L8.5 10"/></svg>)";

        case UiIcon::Duplicate:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M9 15C9 12.1716 9 10.7574 9.87868 9.87868C10.7574 9 12.1716 9 15 9L16 9C18.8284 9 20.2426 9 21.1213 9.87868C22 10.7574 22 12.1716 22 15V16C22 18.8284 22 20.2426 21.1213 21.1213C20.2426 22 18.8284 22 16 22H15C12.1716 22 10.7574 22 9.87868 21.1213C9 20.2426 9 18.8284 9 16L9 15Z"/><path d="M16.9999 9C16.9975 6.04291 16.9528 4.51121 16.092 3.46243C15.9258 3.25989 15.7401 3.07418 15.5376 2.90796C14.4312 2 12.7875 2 9.5 2C6.21252 2 4.56878 2 3.46243 2.90796C3.25989 3.07417 3.07418 3.25989 2.90796 3.46243C2 4.56878 2 6.21252 2 9.5C2 12.7875 2 14.4312 2.90796 15.5376C3.07417 15.7401 3.25989 15.9258 3.46243 16.092C4.51121 16.9528 6.04291 16.9975 9 16.9999"/></svg>)";

        case UiIcon::Delete:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M19.5 5.5L18.8803 15.5251C18.7219 18.0864 18.6428 19.3671 18.0008 20.2879C17.6833 20.7431 17.2747 21.1273 16.8007 21.416C15.8421 22 14.559 22 11.9927 22C9.42312 22 8.1383 22 7.17905 21.4149C6.7048 21.1257 6.296 20.7408 5.97868 20.2848C5.33688 19.3626 5.25945 18.0801 5.10461 15.5152L4.5 5.5"/><path d="M3 5.5H21M16.0557 5.5L15.3731 4.09173C14.9196 3.15626 14.6928 2.68852 14.3017 2.39681C14.215 2.3321 14.1231 2.27454 14.027 2.2247C13.5939 2 13.0741 2 12.0345 2C10.9688 2 10.436 2 9.99568 2.23412C9.8981 2.28601 9.80498 2.3459 9.71729 2.41317C9.32164 2.7167 9.10063 3.20155 8.65861 4.17126L8.05292 5.5"/><path d="M9.5 16.5L9.5 10.5"/><path d="M14.5 16.5L14.5 10.5"/></svg>)";

        case UiIcon::Magnet:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M3.925 2.5H9.15C9.15 2.5 8.2 7.7 8.2 10.1C8.2 13.1 10.1 15.5 12 15.5C13.9 15.5 15.8 13.1 15.8 10.1C15.8 7.7 14.85 2.5 14.85 2.5H20.075C20.075 2.5 21.5 7.13842 21.5 11.5C21.5 17 17.7 21.5 12 21.5C6.3 21.5 2.5 17 2.5 11.5C2.5 7.13842 3.925 2.5 3.925 2.5Z"/><path d="M2.97266 7.25H8.19766M15.7977 7.25L21.0227 7.25"/></svg>)";

        case UiIcon::Play:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M18.8906 12.846C18.5371 14.189 16.8667 15.138 13.5257 17.0361C10.296 18.8709 8.6812 19.7884 7.37983 19.4196C6.8418 19.2671 6.35159 18.9776 5.95624 18.5787C5 17.6139 5 15.7426 5 12C5 8.2574 5 6.3861 5.95624 5.42132C6.35159 5.02245 6.8418 4.73288 7.37983 4.58042C8.6812 4.21165 10.296 5.12907 13.5257 6.96393C16.8667 8.86197 18.5371 9.811 18.8906 11.154C19.0365 11.7084 19.0365 12.2916 18.8906 12.846Z"/></svg>)";

        case UiIcon::Pause:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M4 7C4 5.58579 4 4.87868 4.43934 4.43934C4.87868 4 5.58579 4 7 4C8.41421 4 9.12132 4 9.56066 4.43934C10 4.87868 10 5.58579 10 7V17C10 18.4142 10 19.1213 9.56066 19.5607C9.12132 20 8.41421 20 7 20C5.58579 20 4.87868 20 4.43934 19.5607C4 19.1213 4 18.4142 4 17V7Z"/><path d="M14 7C14 5.58579 14 4.87868 14.4393 4.43934C14.8787 4 15.5858 4 17 4C18.4142 4 19.1213 4 19.5607 4.43934C20 4.87868 20 5.58579 20 7V17C20 18.4142 20 19.1213 19.5607 19.5607C19.1213 20 18.4142 20 17 20C15.5858 20 14.8787 20 14.4393 19.5607C14 19.1213 14 18.4142 14 17V7Z"/></svg>)";

        case UiIcon::StepBack:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="19 20 9 12 19 4 19 20"/><line x1="5" y1="19" x2="5" y2="5"/></svg>)";

        case UiIcon::StepForward:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="5 4 15 12 5 20 5 4"/><line x1="19" y1="5" x2="19" y2="19"/></svg>)";

        case UiIcon::SkipBack:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="11 19 2 12 11 5 11 19"/><polygon points="22 19 13 12 22 5 22 19"/></svg>)";

        case UiIcon::SkipForward:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="13 5 22 12 13 19 13 5"/><polygon points="2 5 11 12 2 19 2 5"/></svg>)";

        case UiIcon::ZoomIn:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M17 17L21 21"/><path d="M19 11C19 6.58172 15.4183 3 11 3C6.58172 3 3 6.58172 3 11C3 15.4183 6.58172 19 11 19C15.4183 19 19 15.4183 19 11Z"/><path d="M7.5 11L14.5 11M11 7.5V14.5"/></svg>)";

        case UiIcon::ZoomOut:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M17 17L21 21"/><path d="M19 11C19 6.58172 15.4183 3 11 3C6.58172 3 3 6.58172 3 11C3 15.4183 6.58172 19 11 19C15.4183 19 19 15.4183 19 11Z"/><path d="M7.5 11L14.5 11"/></svg>)";

        case UiIcon::Undo:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 7v6h6"/><path d="M21 17a9 9 0 0 0-9-9 9 9 0 0 0-6 2.3L3 13"/></svg>)";

        case UiIcon::Redo:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 7v6h-6"/><path d="M3 17a9 9 0 0 1 9-9 9 9 0 0 1 6 2.3l3 2.7"/></svg>)";

        case UiIcon::Export:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M17.9999 21.9999C19.4001 21.9999 20.1001 21.9999 20.6349 21.7275C21.1053 21.4878 21.4878 21.1053 21.7275 20.6349C21.9999 20.1001 21.9999 19.4001 21.9999 17.9999C21.9999 16.5998 21.9999 15.8997 21.7275 15.365C21.4878 14.8946 21.1053 14.5121 20.6349 14.2724C20.1001 13.9999 19.4001 13.9999 17.9999 13.9999L5.99994 13.9999C4.59981 13.9999 3.89974 13.9999 3.36496 14.2724C2.89456 14.5121 2.51211 14.8946 2.27242 15.365C1.99994 15.8997 1.99994 16.5998 1.99994 17.9999C1.99994 19.4001 1.99994 20.1001 2.27242 20.6349C2.51211 21.1053 2.89456 21.4878 3.36496 21.7275C3.89974 21.9999 4.59981 21.9999 5.99994 21.9999L17.9999 21.9999Z"/><path d="M11.9999 5.99994L11.9999 13.9999M11.9999 5.99994C11.2997 5.99994 9.99147 7.99424 9.49994 8.49994M11.9999 5.99994C12.7002 5.99994 14.0084 7.99424 14.4999 8.49994"/><path d="M1.99994 7.99994C1.99994 5.66105 1.99994 4.49161 2.5364 3.63783C2.81615 3.19261 3.19261 2.81615 3.63783 2.5364C4.49161 1.99994 5.66105 1.99994 7.99994 1.99994L15.9999 1.99994C18.3388 1.99994 19.5083 1.99994 20.3621 2.5364C20.8073 2.81615 21.1837 3.19261 21.4635 3.63783C21.9999 4.49161 21.9999 5.66105 21.9999 7.99994"/></svg>)";

        case UiIcon::Plus:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>)";

        case UiIcon::Eye:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7Z"/><circle cx="12" cy="12" r="3"/></svg>)";

        case UiIcon::EyeOff:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9.88 9.88a3 3 0 1 0 4.24 4.24"/><path d="M10.73 5.08A10.43 10.43 0 0 1 12 5c7 0 10 7 10 7a13.16 13.16 0 0 1-1.67 2.68"/><path d="M6.61 6.61A13.526 13.526 0 0 0 2 12s3 7 10 7a9.74 9.74 0 0 0 5.39-1.61"/><line x1="2" y1="2" x2="22" y2="22"/></svg>)";

        case UiIcon::Volume:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/><path d="M15.54 8.46a5 5 0 0 1 0 7.07"/><path d="M19.07 4.93a10 10 0 0 1 0 14.14"/></svg>)";

        case UiIcon::VolumeMute:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/><line x1="23" y1="9" x2="17" y2="15"/><line x1="17" y1="9" x2="23" y2="15"/></svg>)";

        case UiIcon::Search:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>)";

        case UiIcon::SplitLeft:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M8.00232 7.5C8.00232 6.65611 7.91895 5.68211 8.75232 5.20096C9.10039 5 9.5677 5 10.5023 5H11.5023C12.4369 5 12.9042 5 13.2523 5.20096C14.0857 5.68211 14.0023 6.65611 14.0023 7.5C14.0023 8.34389 14.0857 9.31789 13.2523 9.79904C12.9042 10 12.4369 10 11.5023 10H10.5023C9.5677 10 9.10039 10 8.75232 9.79904C7.91895 9.31789 8.00232 8.34389 8.00232 7.5Z"/><path d="M8.00232 16.5C8.00232 15.6561 7.91895 14.6821 8.75232 14.201C9.10039 14 9.5677 14 10.5023 14H17.5023C18.4369 14 18.9042 14 19.2523 14.201C20.0857 14.6821 20.0023 15.6561 20.0023 16.5C20.0023 17.3439 20.0857 18.3179 19.2523 18.799C18.9042 19 18.4369 19 17.5023 19H10.5023C9.5677 19 9.10039 19 8.75232 18.799C7.91895 18.3179 8.00232 17.3439 8.00232 16.5Z"/><path d="M4 2V22"/></svg>)";

        case UiIcon::SplitRight:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M10.0004 7.5C10.0004 6.65611 9.91699 5.68211 10.7504 5.20096C11.0984 5 11.5657 5 12.5004 5H13.5004C14.435 5 14.9023 5 15.2504 5.20096C16.0837 5.68211 16.0004 6.65611 16.0004 7.5C16.0004 8.34389 16.0837 9.31789 15.2504 9.79904C14.9023 10 14.435 10 13.5004 10H12.5004C11.5657 10 11.0984 10 10.7504 9.79904C9.91699 9.31789 10.0004 8.34389 10.0004 7.5Z"/><path d="M4.00036 16.5C4.00036 15.6561 3.91699 14.6821 4.75036 14.201C5.09844 14 5.56575 14 6.50036 14H13.5004C14.435 14 14.9023 14 15.2504 14.201C16.0837 14.6821 16.0004 15.6561 16.0004 16.5C16.0004 17.3439 16.0837 18.3179 15.2504 18.799C14.9023 19 14.435 19 13.5004 19H6.50036C5.56575 19 5.09844 19 4.75036 18.799C3.91699 18.3179 4.00036 17.3439 4.00036 16.5Z"/><path d="M20 2V22"/></svg>)";

        case UiIcon::Unlink:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M16.8463 14.6095L19.4558 12C21.5147 9.94113 21.5147 6.60303 19.4558 4.54416C17.397 2.48528 14.0589 2.48528 12 4.54416L9.39045 7.1537M14.6095 16.8463L12 19.4558C9.94113 21.5147 6.60303 21.5147 4.54416 19.4558C2.48528 17.397 2.48528 14.0589 4.54416 12L7.1537 9.39045"/><path d="M22 17H19.9211M17 22L17 19.9211"/><path d="M2 7H4.07889M7 2L7 4.07889"/></svg>)";

        case UiIcon::Link:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M9.5 14.5L14.5 9.49995"/><path d="M16.8463 14.6095L19.4558 12C21.5147 9.94108 21.5147 6.60298 19.4558 4.54411C17.397 2.48524 14.0589 2.48524 12 4.54411L9.39045 7.15366M14.6095 16.8463L12 19.4558C9.94113 21.5147 6.60303 21.5147 4.54416 19.4558C2.48528 17.3969 2.48528 14.0588 4.54416 12L7.1537 9.39041"/></svg>)";

        case UiIcon::Adjustment:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M3.99963 5.00055L9.99963 5.00031"/><path d="M12.9996 5.00031L19.9996 5.00031"/><path d="M15.9996 9.00031L15.9996 15.0003"/><path d="M9.99963 2.00031L9.99963 8.00031"/><path d="M11.9996 16.0003L11.9996 22.0003"/><path d="M15.9996 12.0001L19.9996 12.0003"/><path d="M3.99963 12.0005L12.9996 12.0003"/><path d="M11.9996 19.0003L19.9996 19.0003"/><path d="M3.99963 19.0005L8.99963 19.0003"/></svg>)";

        case UiIcon::Fullscreen:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M15.5 21C16.8956 21 17.5933 21 18.1611 20.8278C19.4395 20.44 20.44 19.4395 20.8278 18.1611C21 17.5933 21 16.8956 21 15.5M21 8.5C21 7.10444 21 6.40666 20.8278 5.83886C20.44 4.56046 19.4395 3.56004 18.1611 3.17224C17.5933 3 16.8956 3 15.5 3M8.5 21C7.10444 21 6.40666 21 5.83886 20.8278C4.56046 20.44 3.56004 19.4395 3.17224 18.1611C3 17.5933 3 16.8956 3 15.5M3 8.5C3 7.10444 3 6.40666 3.17224 5.83886C3.56004 4.56046 4.56046 3.56004 5.83886 3.17224C6.40666 3 7.10444 3 8.5 3"/></svg>)";

        case UiIcon::Target:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="6"/><circle cx="12" cy="12" r="2"/></svg>)";

        case UiIcon::Chart:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M3 4V14C3 16.8284 3 18.2426 3.87868 19.1213C4.75736 20 6.17157 20 9 20H21"/><path d="M6 14L9.25 10.75C9.89405 10.1059 10.2161 9.78392 10.5927 9.67766C10.8591 9.60254 11.1409 9.60254 11.4073 9.67766C11.7839 9.78392 12.1059 10.1059 12.75 10.75C13.3941 11.3941 13.7161 11.7161 14.0927 11.8223C14.3591 11.8975 14.6409 11.8975 14.9073 11.8223C15.2839 11.7161 15.6059 11.3941 16.25 10.75L20 7"/></svg>)";

        case UiIcon::ShapeRect:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="5" width="18" height="14" rx="2"/></svg>)";

        case UiIcon::ShapeCircle:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="9"/></svg>)";

        case UiIcon::ShapeStar:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2"/></svg>)";

        case UiIcon::ShapeArrow:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/></svg>)";

        case UiIcon::Sun:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/></svg>)";

        case UiIcon::Contrast:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><path d="M12 18a6 6 0 0 0 0-12v12z"/></svg>)";

        case UiIcon::Sliders:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/></svg>)";

        case UiIcon::Grid:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/></svg>)";

        case UiIcon::Keyboard:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="4" width="20" height="16" rx="2"/><path d="M6 8h.001M10 8h.001M14 8h.001M18 8h.001M6 12h.001M10 12h.001M14 12h.001M18 12h.001M7 16h10"/></svg>)";

        case UiIcon::Check:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg>)";

        case UiIcon::List:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="8" y1="6" x2="21" y2="6"/><line x1="8" y1="12" x2="21" y2="12"/><line x1="8" y1="18" x2="21" y2="18"/><line x1="3" y1="6" x2="3.01" y2="6"/><line x1="3" y1="12" x2="3.01" y2="12"/><line x1="3" y1="18" x2="3.01" y2="18"/></svg>)";

        case UiIcon::Clock:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/></svg>)";

        case UiIcon::Folder:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 20h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.93a2 2 0 0 1-1.66-.9l-.82-1.2A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13c0 1.1.9 2 2 2Z"/></svg>)";

        case UiIcon::Edit:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/></svg>)";

        case UiIcon::MoreHorizontal:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="1"/><circle cx="19" cy="12" r="1"/><circle cx="5" cy="12" r="1"/></svg>)";

        case UiIcon::ArrowDown:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="5" x2="12" y2="19"/><polyline points="19 12 12 19 5 12"/></svg>)";

        case UiIcon::ArrowUp:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="19" x2="12" y2="5"/><polyline points="5 12 12 5 19 12"/></svg>)";

        case UiIcon::Info:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>)";

        case UiIcon::Logo:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" fill="none"><path d="M32 9.37305V22.627L22.627 32H9.37305L0 22.627V9.37305L9.37305 0H22.627L32 9.37305ZM8 8V24H24V8H8Z" fill="%1"/></svg>)";

        case UiIcon::Upload:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M2.99994 17C2.99994 17.93 2.99994 18.395 3.10216 18.7765C3.37956 19.8117 4.18821 20.6204 5.22348 20.8978C5.60498 21 6.06997 21 6.99994 21L16.9999 21C17.9299 21 18.3949 21 18.7764 20.8978C19.8117 20.6204 20.6203 19.8117 20.8977 18.7765C20.9999 18.395 20.9999 17.93 20.9999 17"/><path d="M16.5 7.49993C16.5 7.49993 13.1858 2.99997 12 2.99996C10.8141 2.99995 7.50002 7.49996 7.50002 7.49996M12 3.99996V16"/></svg>)";

        case UiIcon::Ripple:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M5 9C4.39316 9.58984 2 11.1597 2 12C2 12.8403 4.39316 14.4102 5 15"/><path d="M19 9C19.6068 9.58984 22 11.1597 22 12C22 12.8403 19.6068 14.4102 19 15"/><path d="M2.42285 11.9795H21.868"/><path d="M12.1454 21.7021L12.1454 2.25692"/></svg>)";

        case UiIcon::Bookmark:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M4 17.9808V9.70753C4 6.07416 4 4.25748 5.17157 3.12874C6.34315 2 8.22876 2 12 2C15.7712 2 17.6569 2 18.8284 3.12874C20 4.25748 20 6.07416 20 9.70753V17.9808C20 20.2867 20 21.4396 19.2272 21.8523C17.7305 22.6514 14.9232 19.9852 13.59 19.1824C12.8168 18.7168 12.4302 18.484 12 18.484C11.5698 18.484 11.1832 18.7168 10.41 19.1824C9.0768 19.9852 6.26947 22.6514 4.77285 21.8523C4 21.4396 4 20.2867 4 17.9808Z"/></svg>)";

        case UiIcon::Video:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 29 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round"><path d="M2.41667 11C2.41667 7.70017 2.41667 6.05025 3.65537 5.02513C4.89405 4 6.88771 4 10.875 4H12.0833C16.0706 4 18.0642 4 19.303 5.02513C20.5417 6.05025 20.5417 7.70017 20.5417 11V13C20.5417 16.2998 20.5417 17.9497 19.303 18.9749C18.0642 20 16.0706 20 12.0833 20H10.875C6.88771 20 4.89405 20 3.65537 18.9749C2.41667 17.9497 2.41667 16.2998 2.41667 13V11Z"/><path d="M20.5417 8.90585L20.6938 8.80196C23.2504 7.05623 24.5287 6.18336 25.556 6.60482C26.5833 7.02628 26.5833 8.42355 26.5833 11.2181V12.7819C26.5833 15.5765 26.5833 16.9737 25.556 17.3952C24.5287 17.8166 23.2504 16.9438 20.6938 15.198L20.5417 15.0941"/></svg>)";

        case UiIcon::Layers:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M8.64298 3.14559L6.93816 3.93362C4.31272 5.14719 3 5.75397 3 6.75C3 7.74603 4.31272 8.35281 6.93817 9.56638L8.64298 10.3544C10.2952 11.1181 11.1214 11.5 12 11.5C12.8786 11.5 13.7048 11.1181 15.357 10.3544L17.0618 9.56638C19.6873 8.35281 21 7.74603 21 6.75C21 5.75397 19.6873 5.14719 17.0618 3.93362L15.357 3.14559C13.7048 2.38186 12.8786 2 12 2C11.1214 2 10.2952 2.38186 8.64298 3.14559Z"/><path d="M20.788 11.0972C20.9293 11.2959 21 11.5031 21 11.7309C21 12.7127 19.6873 13.3109 17.0618 14.5072L15.357 15.284C13.7048 16.0368 12.8786 16.4133 12 16.4133C11.1214 16.4133 10.2952 16.0368 8.64298 15.284L6.93817 14.5072C4.31272 13.3109 3 12.7127 3 11.7309C3 11.5031 3.07067 11.2959 3.212 11.0972"/><path d="M20.3767 16.2661C20.7922 16.5971 21 16.927 21 17.3176C21 18.2995 19.6873 18.8976 17.0618 20.0939L15.357 20.8707C13.7048 21.6236 12.8786 22 12 22C11.1214 22 10.2952 21.6236 8.64298 20.8707L6.93817 20.0939C4.31272 18.8976 3 18.2995 3 17.3176C3 16.927 3.20778 16.5971 3.62334 16.2661"/></svg>)";

        case UiIcon::Snow:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="m10 20-1.25-2.5L6 18"/><path d="M10 4 8.75 6.5 6 6"/><path d="m14 20 1.25-2.5L18 18"/><path d="m14 4 1.25 2.5L18 6"/><path d="m17 21-3-6h-4"/><path d="m17 3-3 6h-4"/><path d="m2 12 2.5 1.25L4 16"/><path d="m2 12 2.5-1.25L4 8"/><path d="m20 10 1.5 2-1.5 2"/><path d="m22 12-6-3v-4"/><path d="m22 12-6 3v4"/><path d="m4 12 6-3v-4"/><path d="m4 12 6 3v4"/></svg>)";

        case UiIcon::SortOneNine:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M7 9.99936V3.94812C7 3.3736 7 3.08634 6.76959 3.01485C6.26306 2.85769 5.5 3.99902 5.5 3.99902M7 9.99936H5.5M7 9.99936H8.5"/><path d="M9 17.4991V15.7491C9 14.9242 9 14.5117 8.70711 14.2554C8.41421 13.9991 7.94281 13.9991 7 13.9991C6.05719 13.9991 5.58579 13.9991 5.29289 14.2554C5 14.5117 5 14.9242 5 15.7491C5 16.5741 5 16.9866 5.29289 17.2429C5.58579 17.4991 6.05719 17.4991 7 17.4991H9ZM9 17.4991V18.3741C9 19.6116 9 20.2303 8.56066 20.6147C8.12132 20.9991 7.41421 20.9991 6 20.9991H5"/><path d="M16.5 19.9991V3.99915M16.5 19.9991C15.7998 19.9991 14.4915 18.0048 14 17.4991M16.5 19.9991C17.2002 19.9991 18.5085 18.0048 19 17.4991"/></svg>)";

        case UiIcon::SlidersBox:
            return R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M2.5 12C2.5 7.52166 2.5 5.28249 3.89124 3.89124C5.28249 2.5 7.52166 2.5 12 2.5C16.4783 2.5 18.7175 2.5 20.1088 3.89124C21.5 5.28249 21.5 7.52166 21.5 12C21.5 16.4783 21.5 18.7175 20.1088 20.1088C18.7175 21.5 16.4783 21.5 12 21.5C7.52166 21.5 5.28249 21.5 3.89124 20.1088C2.5 18.7175 2.5 16.4783 2.5 12Z" stroke-linejoin="round"/><path d="M8.5 10C7.67157 10 7 9.32843 7 8.5C7 7.67157 7.67157 7 8.5 7C9.32843 7 10 7.67157 10 8.5C10 9.32843 9.32843 10 8.5 10Z"/><path d="M15.5 17C16.3284 17 17 16.3284 17 15.5C17 14.6716 16.3284 14 15.5 14C14.6716 14 14 14.6716 14 15.5C14 16.3284 14.6716 17 15.5 17Z"/><path d="M10 8.5L17 8.5"/><path d="M14 15.5L7 15.5"/></svg>)";
    }
    return "";
}

QPixmap UiIcons::getPixmap(UiIcon icon, const QColor& color, int size) {
    const qreal ratio = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    const auto key = QString("catchim-icon-%1-%2-%3-%4").arg(static_cast<int>(icon)).arg(color.rgba()).arg(size).arg(ratio);
    QPixmap cached;
    if (QPixmapCache::find(key, &cached)) return cached;
    const char* rawTemplate = getSvgContent(icon);
    QString svgString = QString(rawTemplate).arg(color.name());
    svgString.replace("stroke-width=\"2\"","stroke-width=\"1.5\"");
    if (icon == UiIcon::SlidersBox) svgString.replace("stroke-width=\"1.5\"", "stroke-width=\"1\"");

    QByteArray svgBytes = svgString.toUtf8();
    QSvgRenderer renderer(svgBytes);

    QPixmap pixmap(static_cast<int>(std::ceil(size * ratio)), static_cast<int>(std::ceil(size * ratio)));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setOpacity(color.alphaF());
    renderer.render(&painter, QRectF(0, 0, size, size));
    painter.end();
    QPixmapCache::insert(key, pixmap);

    return pixmap;
}

QIcon UiIcons::get(UiIcon icon, const QColor& color, int size) {
    QIcon qicon;
    qicon.addPixmap(getPixmap(icon, color, size), QIcon::Normal);

    // Hover state lighter or sky blue
    // Hover is expressed by the button surface. Keep vector icons legible
    // instead of washing their strokes out against the light theme.
    QColor hoverColor = color;
    qicon.addPixmap(getPixmap(icon, hoverColor, size), QIcon::Active);

    // Disabled state
    QColor disabledColor = color;
    disabledColor.setAlphaF(color.alphaF() * 0.5);
    qicon.addPixmap(getPixmap(icon, disabledColor, size), QIcon::Disabled);

    return qicon;
}

} // namespace catchim::ui
#endif
