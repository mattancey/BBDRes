#pragma once

// Citations de la fenêtre de droite, et règles de mise en forme (C++ standard uniquement, testées hors du plug-in).
// Usage personnel : le texte est conservé tel quel (rien n'est dédoublonné ni attribué). Seule la PRÉSENTATION est
// normalisée à l'affichage : guillemets extérieurs retirés, apostrophes typographiques, espaces insécables
// avant ; : ! ?, guillemets droits remplacés par des guillemets français.

#include <algorithm>
#include <string>
#include <vector>

namespace bbdquotes
{
inline const char* const kRaw = R"QUOTES(
« Aimer, ce n’est pas se regarder l’un l’autre, mais regarder ensemble dans la même direction. »
« On ne voit bien qu’avec le cœur. L’essentiel est invisible pour les yeux. »
« Tu deviens responsable pour toujours de ce que tu as apprivoisé. »
« C’est le temps que tu as perdu pour ta rose qui fait ta rose si importante. »
« Ce qui rend le désert beau, c’est qu’il cache quelque part un puits. »
« Toutes les grandes personnes ont d’abord été des enfants, mais peu d’entre elles s’en souviennent. »
« Les mots sont source de malentendus. »
« Il est bien plus difficile de se juger soi-même que de juger autrui. »
« Les hommes ont oublié cette vérité, mais tu ne dois pas l’oublier. »
« Tu es responsable de ta rose. »
« Les étoiles sont belles à cause d’une fleur que l’on ne voit pas. »
« Aimer, ce n’est pas se regarder l’un l’autre, mais regarder ensemble dans la même direction. »
« La perfection est atteinte non pas lorsqu’il n’y a plus rien à ajouter, mais lorsqu’il n’y a plus rien à retirer. »
« Fais de ta vie un rêve, et d’un rêve une réalité. »
« L’avenir porte plusieurs noms. »
« Le vrai bonheur naît de la joie du travail bien fait. »
« Être homme, c’est précisément être responsable. »
« Nous n’héritons pas de la Terre de nos ancêtres, nous l’empruntons à nos enfants. »
« L’homme se découvre lorsqu’il se mesure à un obstacle. »
« Ce qui sauve l’homme, c’est de faire un pas. Puis un autre pas. »
« La machine n’isole pas l’homme des grands problèmes de la nature. »
« Le sens des choses ne réside pas dans les choses elles-mêmes, mais dans notre attitude à leur égard. »
« Il faut regarder avec le cœur. »
« Les yeux sont aveugles. Il faut chercher avec le cœur. »
« C’est seulement avec le cœur que l’on peut voir correctement. »
« Tu es belle, mais tu es vide. »
« Les plus belles choses du monde ne peuvent être ni vues ni touchées. »
« Les étoiles ne sont pas les mêmes pour tout le monde. »
« Chaque homme doit chercher en lui-même le sens de sa vie. »
« Le seul véritable luxe est celui des relations humaines. »
« Nous sommes tous responsables de tous. »
« Être homme, c’est se sentir responsable. »
« Une civilisation se construit sur ce que l’on exige des hommes. »
« Seul l’inconnu effraie les hommes. »
« Une fois que l’homme a affronté l’inconnu, cette terreur devient une chose connue. »
« Je ne connais qu’une seule liberté : celle de l’esprit. »
« Celui qui veut voyager heureux doit voyager léger. »
« Un objectif sans plan n’est qu’un souhait. »
« C’est l’esprit qui mène le monde, et non l’intelligence. »
« La vie nous a appris que l’amour ne consiste pas à se regarder l’un l’autre. »
« Il faut donner avant de recevoir. »
« La Terre nous en apprend davantage sur nous-mêmes que tous les livres. »
« Être vivant, c’est être transformé. »
« L’homme n’est qu’un réseau de relations. »
« Les êtres humains sont reliés par des fils invisibles. »
« Ce qui importe, ce n’est pas ce que nous regardons, mais ce que nous voyons. »
« Chaque homme porte en lui un monde de rêves. »
« La liberté est la possibilité de devenir meilleur. »
« Créer, c’est donner une forme à son destin. »
« Chaque voyage commence par un premier pas. »
« Ta tâche n’est pas de prévoir l’avenir, mais de le rendre possible. »
« L’essentiel est invisible pour les yeux. »
« Les mots peuvent parfois être la source de grands malentendus. »
« Seul le cœur sait ce qui est véritablement important. »
« Le désert est beau parce qu’il cache quelque chose de précieux. »
« Il faut exiger de chacun le devoir que chacun peut accomplir. »
« L’autorité repose avant tout sur la raison. »
« Le véritable amour commence là où l’on n’attend rien en retour. »
« L’amitié naît des expériences partagées. »
« Être humain, c’est partager un destin commun. »
« Nous devons apprendre à regarder au-delà des apparences. »
« Chaque homme est responsable du monde qu’il crée. »
« Les plus grandes victoires sont souvent invisibles. »
« Comprendre, c’est découvrir les liens entre les choses. »
« L’homme se mesure aux obstacles qu’il surmonte. »
« Tout acte de création commence par un rêve. »
« La valeur des choses réside dans le sens que nous leur donnons. »
« La vie est un voyage continu vers la compréhension. »
« L’âme grandit à travers ce qu’elle expérimente. »
« Il ne faut jamais confondre l’essentiel avec le visible. »
« Vivre, c’est construire des ponts entre les êtres humains. »
« L’humanité se renforce par la solidarité. »
« La plus grande richesse est celle des relations humaines. »
« Les rêves donnent un sens à notre existence. »
« Être responsable, c’est s’engager envers le monde. »
« Chaque étoile brille pour quelqu’un. »
« Parfois, les choses les plus importantes sont les plus difficiles à exprimer. »
« La véritable beauté se trouve dans ce qui ne peut être vu. »
« Chaque être humain a quelque chose d’unique à offrir. »
« L’espoir naît lorsque nous osons avancer. »
« Il faut du courage pour se découvrir soi-même. »
« Le monde appartient à ceux qui osent l’imaginer autrement. »
« La vie prend tout son sens lorsqu’elle est partagée. »
« Créer quelque chose de significatif, c’est offrir une part de soi-même. »
« L’inconnu n’est pas nécessairement quelque chose à craindre. »
« Chaque obstacle est une occasion de découvrir notre force. »
« Les plus grands voyages sont ceux qui nous transforment. »
« Les relations humaines sont la plus grande richesse. »
« Comprendre une autre personne, c’est découvrir un nouveau monde. »
« Chaque instant peut prendre un sens grâce à l’amour. »
« La véritable liberté commence en nous-mêmes. »
« La beauté de la vie réside dans ses liens invisibles. »
« Il ne faut jamais perdre sa capacité à s’émerveiller. »
« Rêver, c’est se donner une raison d’avancer. »
« Le cœur comprend ce que la raison ne peut expliquer. »
« Chaque personne que nous rencontrons laisse quelque chose en nous. »
« La vie ne se mesure pas à sa longueur, mais à sa profondeur. »
« Les choses les plus précieuses sont celles que nous ne pouvons posséder. »
« L’amour donne un sens à tout ce que nous faisons. »
« Être humain, c’est prendre soin de quelque chose qui nous dépasse. »
« Le voyage le plus important est peut-être celui qui nous ramène à nous-mêmes. »
Sur ce sentiment inconnu, dont l'ennui, la douceur m'obsèdent, j'hésite à apposer le nom, le beau nom grave de tristesse. »
« L'insouciance est le seul sentiment qui puisse inspirer notre vie et ne pas disposer d'arguments pour se défendre. »
« On s'habitue aux défauts des autres quand on ne croit pas de son devoir de les corriger. »
« Quand on est ivre, on dit la vérité et personne ne vous croit. »
« Vous pensez peu au futur, n'est-ce pas ? C'est le privilège de la jeunesse. »
« Bien sûr on a des chagrins d'amour, mais on a surtout des chagrins de soi-même. Finalement la vie n'est qu'une affaire de solitude. »
« La liberté de penser, et de mal penser et de penser peu, la liberté de choisir moi-même ma vie, de me choisir moi-même. »
« Parfois on éprouve le besoin de fuir, de s'échapper, de se perdre. Mais d'autres fois, on se perd sans le vouloir, au hasard d'un excès. »
« Vous vous faites de l'amour une idée un peu simpliste. Ce n'est pas une suite de sensations indépendantes les unes des autres. C'est autre chose. Il y a la tendresse constante, la douceur, le manque… »
« J'éprouvais, en dehors du plaisir physique et très réel que me procurait l'amour, une sorte de plaisir intellectuel à y penser. »
« Nous étions de la même race, lui et moi ; je me disais tantôt que c'était la belle race pure des nomades, tantôt la race pauvre et desséchée des jouisseurs. »
« Un jour, j'aimerais quelqu'un passionnément et je lui chercherais un chemin vers lui, ainsi, avec précaution, avec douceur, la main tremblante… »
« La vie recommença comme avant, comme il était prévu qu'elle recommencerait. »
« Bertrand était mon premier amant. C'était sur lui que j'avais connu le parfum de mon propre corps. C'est toujours sur le corps des autres qu'on découvre le sien, sa longueur, son odeur, d'abord avec méfiance, puis avec reconnaissance. »
« Vivre, au fond, c'était s'arranger pour être le plus content possible. Et ce n'était pas si facile. »
« Je ne sais pas si c'est l'amour ou l'entente ; ça n'a pas d'importance. Nous sommes seuls, chacun de notre côté. »
« Bertrand passait son temps à chercher les comédies chez les autres, à tel point qu'il vivait un peu dans la crainte de se jouer lui-même une comédie dont il ne serait pas conscient. »
« On disait des Maligrasse, avec sympathie, qu'ils aimaient la jeunesse et cette sympathie était pour une fois justifiée. »
« L'amour comme une catastrophe s'il n'est pas aussitôt partagé. »
« Dans un mois, dans un an, comment souffrirons-nous ? »
« Il ne s'agissait plus de trouver une réponse mais d'attendre que la question ne se posât plus. »
« On se remet de tout. — Non. Il y a eu entre toi et moi quelque chose d'inexorable, tu le sens bien. »
« On est assez mesquins avec les morts : à peine le sont-ils qu'on les enferme dans des boîtes noires, bien fermées, puis dans la terre. On s'en débarrasse. »
« Quel mur s'interpose donc toujours entre les êtres humains et leur désir le plus intime, leur effroyable volonté de bonheur ? »
« C'est très astreignant, le bonheur, on ne peut plus s'y dérober qu'à la neurasthénie. »
« Des galopins qui sentent encore le lait n'ont pas à se blottir dans les bras des dames qui sentent le scotch. »
« Enfin ce qu'on appelle la folie et qui m'a toujours paru la seule forme sensée de l'amour. »
« Il est facile d'être bien dans sa peau quand quelqu'un a envie de toucher cette peau. »
« Certaines amours m'avaient laissé penser qu'effectivement je ne m'en remettrai jamais. »
« Il préférait avoir été malheureux pour une bonne raison qu'heureux pour une mauvaise. »
« Pourquoi veux-tu que j'aille gâcher mon temps à préparer mon avenir, puisque mon présent seul m'intéresse. Et me comble. »
« Elle se demandait si l'honnêteté, la seule honnêteté possible dans cette vie inextricable, ne consistait pas à aimer quelqu'un suffisamment pour le rendre heureux. »
« Avec vous, j'ai joué le jeune et brillant avocat, et l'amour transi, et l'enfant gâté, et dieu sait quoi. Mais depuis que je vous connais, tous mes rôles sont pour vous. Vous ne pensez pas que c'est l'amour ? »
« Il n'y a qu'un vrai silence : celui de la mort. »
« L'intelligence sans bonté est une arme bien dangereuse. »
« Je t'userai, je m'userai, je ne te quitterai pas, nous n'aurons pas de répit. Deux êtres humains doivent pouvoir vivre cramponnés l'un à l'autre sans respirer, ça s'appelle l'amour. »
« Tout ce que je fais pour moi est contre moi, c'est assez épouvantable. »
« Pour qu'un homme et une femme s'aiment vraiment, il ne suffisait pas qu'ils se soient fait plaisir, qu'ils se soient fait rire, il fallait aussi qu'ils se soient fait souffrir. »
« Il y a des moments de bonheur parfait, quelquefois dans la solitude, dont le souvenir, plus que celui de n'importe qui d'extérieur, peut, en cas de crise, vous sauver du désespoir. »
« On ne peut consoler personne d'être né et d'avoir à mourir. »
« La passion se nourrit de tout, y compris les signes les plus contraires à ses désirs. »
« Il faut que vous compreniez bien que je vous aime. Ne pensez pas que je vais me consoler de vous, ni vous oublier, ni vous remplacer. »
« On dit que tant de gens oublient ce que parler veut dire, mais tant de gens oublient ce que se taire peut signifier de fou, d'extravagant, d'absurde. »
« Il était vrai que Paris était devenue d'une beauté déchirante, bleue, blonde, alanguie, il était vrai qu'il n'y avait pas plus le cœur d'y mourir que celui d'y vivre. »
« Tout compte fait, elle préférait lui mentir que de se mentir à elle-même. »
« Simplement cette erreur, qui jusque-là avait résonné comme une musique de fond dans le film de sa vie, obstiné mais discrète, devenait à cet instant un tam-tam cruel et irrémédiable. »
« L'alcool a parfois des projecteurs impitoyables, définitifs. »
« Il en était à cette exquise période de l'amour où l'on adore se disputer et où l'on ne peut même pas imaginer que ces sujets de tendres batailles puissent être les ferments, les anges annonciateurs de combats moins gais. »
« Les femmes étaient toutes les mêmes, jamais heureuses. Et c'était toujours votre faute. »
« Ma vie est d'ores et déjà une dissertation accélérée et bâclée, celle de la mauvaise élève, celle qui n'a rien su faire de ses citations, sinon, par moments, son propre bonheur, son propre orgueil et ses réjouissances. »
« Ce n'est pas parce que la vie n'est pas élégante qu'il faut se conduire comme elle. »
« La mort — je ne parle pas de la maladie — la mort, je la vois de velours, gantée, noire et, en tout cas, irrémédiable, absolue. »
« Alors me direz-vous, pourquoi écrire ? D'abord pour des raisons sordides : parce que je suis une vieille cigale et que, si je n'écris pas pendant deux ou trois ans, je me fais l'effet d'une dégénérée. »
« Tous les poètes étaient noctambules, alcooliques et détraqués. Vraiment, devrons-nous acheter des actions Shell et des machines à laver pour être bien considérés et pour être sûrs de mourir vieux ? »
« Il y a eu énormément de guillemets dans ma vie, si j'y réfléchis, quelques points d'exclamation (la passion), quelques points d'interrogation (la dépression nerveuse), quelques points de suspension (l'insouciance)… »
« Si on savait pourquoi quelqu'un qu'on aime meurt, ou peut-être plus bêtement, si on savait pourquoi quelqu'un qu'on aime ne vous aime plus, on en saurait des choses ! »
« La paresse est une drogue aussi violente que le travail. »
« À force de bonheur, comment y survivre. »
« Tout ce qui vous perd, et donc vous permet de vous trouver. »
« Ce n'est pas le bon sens qui est la chose au monde la mieux partagée, ce sont les sentiments. »
« Il y a des gens blessés, comme ça, avant même qu'ils n'aient pris des coups et alors qu'ils ont tous les atouts en main. »
« Je ne me remettrai jamais, je le crois vraiment, de sa mort. Car que faire, parfois ? Que penser ? »
« Ce n'est pas que ce soit si important : le soleil est là, dans la paume de ma main. Pas plus qu'on ne doit essayer de garder le temps et l'amour, on ne doit tenter de garder ni le soleil ni la vie. »
« Vingt fois hypothéquée, deux fois presque vendue, lieu de travail pour mes amis travailleurs, refuge pour mes amis amoureux, cette maison vaut aujourd'hui 8 milliards de souvenirs. »
« New York est une ville de plein air, coupée au cordeau, venteuse et saine. New York est une ville fascinante. »
« Je parlais peu à l'un ou à l'autre à ce moment-là. Nous ne nous disions rien de très profond. Mais ces minutes-là, je savais déjà que je les regretterais un jour. »
« Et finalement, que demander d'autre ou de plus à nos sentiments sinon de nous entraîner sans nous laisser le temps de réfléchir ? »
« Bref, elle voulait se croire, et qu'on la croie contraire à ce qu'elle était. Et c'est là un des plus grands malheurs, et l'un des plus répandus de la race humaine, que ce refus de soi-même. »
« Comme tous ceux qui fuient les coups, comme tous les déserteurs du sentiment, il me suffisait d'une petite blessure pour qu'elle s'infectât. »
« J'ai toujours pensé qu'il y avait des familles sur la terre : en plus de ceux qui partagent votre sang et votre enfance, il y a aussi les familles du hasard, ceux que l'on reconnaît confusément comme étant son parent, son pair, son ami, son amant. »
« Dans la vraie vie, on aime mieux surprendre qu'apprendre, encore plus que comprendre. »
« On dit qu'une femme s'habille pour son entourage, les hommes, les copines. En fait, c'est pour soi qu'on s'habille. »
« On ne s'habille pas pour éblouir les autres femmes ou pour les embêter. On s'habille pour se déshabiller. »
« J'aime trop les gens pour ne pas répugner moi-même à leur faire du mal ; et les juger, c'est déjà leur faire mal. »
« Il m'arrive de trouver que la vie est une horrible plaisanterie. Si l'on est tant soit peu sensible, on est écorché partout et tout le temps. »
« Si tout était à recommencer, je recommencerais bien sûr, en évitant quelques broutilles : les accidents de voiture, les séjours à l'hôpital, les chagrins d'amour. Mais je ne renie rien. »
« Je ne dirai jamais assez les charmes de la vie quand on l'aime. Cet incroyable bonheur de se réveiller vivante avec tout ce temps devant soi, toute cette gigantesque journée offerte avant que le sommeil ne vous fige à nouveau dans une pose mortelle, sur l'oreiller. »
« Un ami qui comprend nos larmes a beaucoup plus de valeur que plein d'amis qui ne comprennent que notre sourire. »
« Mon passe-temps favori, c'est laisser passer le temps, avoir du temps, prendre son temps, perdre son temps, vivre à contretemps. »
« La culture, c'est comme la confiture, moins on en a, plus on l'étale. »
« La culture est ce qui reste quand on ne sait rien faire. »
« Aimer quelqu'un, c'est aussi aimer le bonheur de quelqu'un. »
« Aimer, ce n'est pas seulement "aimer bien" ; c'est surtout comprendre. »
« Il n'y a pas d'âge pour réapprendre à vivre. On dirait même qu'on ne fait que ça toute sa vie : repartir, recommencer, respirer à nouveau. »
« La vie, c'est comme les vagues, une mauvaise, une bonne… »
« J'ai trop le désir qu'on respecte ma liberté pour ne pas respecter celle des autres. »
« Quand votre femme vous trompe, on est ridicule si on l'ignore, complaisant si on le sait, et névrosé si on en souffre. »
« Nous sommes peu à penser trop, trop à penser peu. »
« Les mots "faire l'amour" ont une séduction à eux, très verbale, en les séparant de leur sens. »
« Quand on se tue, c'est pour infliger sa mort aux autres. Il est très rare de voir des suicides élégants. »
« On a aussi peu de liberté maintenant qu'il y a vingt ans : faire l'amour était alors interdit aux jeunes filles ; maintenant c'est presque devenu obligatoire. Les tabous sont les mêmes. »
« La terre est peuplée de truqueurs et de bavards, qui se servent des mots comme d'une monnaie qu'ils sauraient fausse. »
« Pour moi, le bonheur, c'est d'abord d'être bien. »
« Quand on est mal parti, il faut essayer de continuer. »
« La terre seule me rassure, quelle que soit la part de boue qu'elle contient. »
« Au théâtre il y a des règles très précises d'unité, on peut donc mettre en scène des gens exceptionnels, des fous. »
« La mémoire est aussi menteuse que l'imagination, et bien plus dangereuse avec ses petits airs studieux. »
« Cette effrayante santé morale que donne l'ambition ! »
« Entre deux personnes, l'enfance, c'est pire que trente ans de mariage. »
« La jeunesse est la seule génération raisonnable. »
)QUOTES";

inline std::string trim (const std::string& s)
{
    const size_t a = s.find_first_not_of (" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr (a, s.find_last_not_of (" \t\r\n") - a + 1);
}

// Toutes les citations, une par ligne, sans retouche
inline std::vector<std::string> all()
{
    std::vector<std::string> v;
    std::string line;
    for (const char ch : std::string (kRaw) + "\n")
    {
        if (ch == '\n') { line = trim (line); if (! line.empty()) v.push_back (line); line.clear(); }
        else line += ch;
    }
    return v;
}

inline void replaceAll (std::string& s, const std::string& from, const std::string& to)
{
    for (size_t p = 0; (p = s.find (from, p)) != std::string::npos; p += to.size())
        s.replace (p, from.size(), to);
}

// Présentation : guillemets extérieurs retirés, apostrophes ’, guillemets droits -> « », espaces insécables avant ; : ! ?
inline std::string normalise (std::string q)
{
    q = trim (q);
    const std::string open = "\xC2\xAB", close = "\xC2\xBB", nbsp = "\xC2\xA0";
    if (q.compare (0, 2, open) == 0) { q.erase (0, 2); q = trim (q); }
    if (q.size() >= 2 && q.compare (q.size() - 2, 2, close) == 0) { q.erase (q.size() - 2); q = trim (q); }
    replaceAll (q, "'", "\xE2\x80\x99");
    std::string out;
    bool opening = true;
    for (const char ch : q)
    {
        if (ch == '"') { out += opening ? open + nbsp : nbsp + close; opening = ! opening; }
        else out += ch;
    }
    q = out;
    for (const char* p : { " ;", " :", " !", " ?" })
        replaceAll (q, p, nbsp + std::string (p + 1));
    return q;
}

inline unsigned nextCodepoint (const std::string& s, size_t i, size_t& len)
{
    const unsigned char c = (unsigned char) s[i];
    if (c < 0x80) { len = 1; return c; }
    if ((c >> 5) == 6 && i + 1 < s.size()) { len = 2; return ((c & 0x1Fu) << 6) | ((unsigned char) s[i + 1] & 0x3Fu); }
    if ((c >> 4) == 14 && i + 2 < s.size())
    {
        len = 3;
        return ((c & 0x0Fu) << 12) | (((unsigned char) s[i + 1] & 0x3Fu) << 6) | ((unsigned char) s[i + 2] & 0x3Fu);
    }
    len = 1;
    return c;
}

// Un début de phrase : majuscule, tiret cadratin (dialogue) ou guillemet ouvrant
inline bool startsSentence (unsigned cp)
{
    return (cp >= 'A' && cp <= 'Z') || (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) || cp == 0x2014 || cp == 0xAB;
}

// Découpe en phrases : fin de phrase (. ! ? …), éventuels signes fermants, un espace, puis un début de phrase
inline std::vector<std::string> splitSentences (const std::string& s)
{
    std::vector<std::string> out;
    size_t start = 0, i = 0;
    while (i < s.size())
    {
        size_t len = 0;
        const unsigned cp = nextCodepoint (s, i, len);
        if (cp == '.' || cp == '!' || cp == '?' || cp == 0x2026)
        {
            size_t j = i + len;
            for (size_t l2 = 0; j < s.size(); j += l2)   // signes fermants éventuels
            {
                const unsigned c2 = nextCodepoint (s, j, l2);
                if (c2 != 0xBB && c2 != ')' && c2 != 0xA0) break;
            }
            size_t k = j;
            while (k < s.size() && s[k] == ' ') ++k;
            if (k > j && k < s.size())
            {
                size_t l3 = 0;
                if (startsSentence (nextCodepoint (s, k, l3)))
                {
                    out.push_back (trim (s.substr (start, j - start)));
                    start = k;
                    i = k;
                    continue;
                }
            }
        }
        i += len;
    }
    const std::string last = trim (s.substr (start));
    if (! last.empty()) out.push_back (last);
    return out;
}

// Ordre de lecture sans répétition : permutation de 0..count-1 fixée par la graine ; pos = rang dans la permutation
inline int quoteIndexFor (unsigned seed, int pos, int count)
{
    std::vector<int> p ((size_t) count);
    for (int i = 0; i < count; ++i) p[(size_t) i] = i;
    unsigned s = seed != 0u ? seed : 1u;
    for (int i = count - 1; i > 0; --i)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        std::swap (p[(size_t) i], p[(size_t) (s % (unsigned) (i + 1))]);
    }
    return p[(size_t) (((pos % count) + count) % count)];
}
} // namespace bbdquotes
