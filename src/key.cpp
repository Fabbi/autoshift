#include <key.hpp>
#include <misc/fsettings.hpp>

#define SC ShiftCode

SC::ShiftCode(uint32_t _id, const QString& _desc, const QString& _code, bool _redeemed):
  id(_id), desc(_desc), code(_code), redeemed(_redeemed)
{}
SC::ShiftCode(const QString& _desc, const QString& _code, bool _redeemed):
  ShiftCode(UINT32_MAX, _desc, _code, _redeemed)
{}
SC::ShiftCode(const QSqlQuery& qry):
  id(qry.value("id").toInt()), desc(qry.value("desc").toString()),
  code(qry.value("code").toString()), redeemed(qry.value("redeemed").toBool())
{}
SC::ShiftCode():
  id(UINT32_MAX), redeemed(false)
{}
bool SC::commit()
{
  // no valid information
  if (id == UINT32_MAX) return false;

  QSqlQuery search(QString("SELECT * FROM keys WHERE id = %1").arg(id));
  search.exec();

  QSqlQuery qry;
  // there is a key with this ID
  if (search.isValid()) {
    // update it
    qry.prepare("UPDATE keys SET description=(:d), key=(:k), redeemed=(:r) "
                  "WHERE id=(:id)");
    qry.bindValue(":id", id);
    qry.bindValue(":d", desc);
    qry.bindValue(":k", code);
    qry.bindValue(":r", redeemed);

  } else {
    // there is no key so create a new entry
    QString current_platform = FSETTINGS["platform"].toString();
    QString current_game = FSETTINGS["game"].toString();
    qry.prepare("INSERT INTO keys(description, key, platform, game, redeemed) "
                  "VAlUES (?,?,?,?,0)");
    qry.addBindValue(desc);
    qry.addBindValue(code);
    qry.addBindValue(current_platform);
    qry.addBindValue(current_game);
    qry.addBindValue(redeemed);
  }

  return qry.exec();
}
#undef SC

#define SCOL ShiftCollection
SCOL::ShiftCollection(Platform platform, bool all)
{
  query(platform, all);
}

void SCOL::query(Platform platform, bool all)
{
  // TODO
}
void SCOL::commit()
{

}
ShiftCollection SCOL::filter(CodePredicate pred)
{}
#undef SCOL
